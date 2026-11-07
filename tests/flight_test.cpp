#include "flight.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    int failures = 0;

    void check(bool condition, const char *what)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << what << '\n';
            ++failures;
        }
    }

    constexpr double mu_earth = 3.986004418e14;
    constexpr double pi = 3.14159265358979323846;

    // Planet at the origin plus a vessel on a circular orbit of the given radius.
    struct Orbit
    {
        sl::FlightWorld world;
        int planet = 0;
        int ship = 0;
        double radius;
        Orbit(double r, sl::FlightIntegrator integrator, double max_substep) : radius(r)
        {
            planet = world.create_body("earth");
            world.body(planet)->gm = mu_earth;
            world.body(planet)->mode = sl::FlightBodyMode::kinematic;
            ship = world.create_body("ship");
            world.body(ship)->position = {r, 0.0, 0.0};
            world.body(ship)->velocity = {0.0, 0.0, -std::sqrt(mu_earth / r)};
            world.integrator = integrator;
            world.max_substep = max_substep;
        }
        double period() const { return 2.0 * pi * std::sqrt(radius * radius * radius / mu_earth); }
        double position_error(double t) const
        {
            const double angle = std::sqrt(mu_earth / (radius * radius * radius)) * t;
            const sl::dvec3 exact(radius * std::cos(angle), 0.0, -radius * std::sin(angle));
            return glm::length(world.body(ship)->position - exact);
        }
    };

    double energy_drift(sl::FlightIntegrator integrator, double dt)
    {
        Orbit orbit(7.0e6, integrator, dt);
        const double e0 = orbit.world.specific_energy(orbit.ship);
        orbit.world.step(orbit.period() * 2.3);
        return std::fabs(orbit.world.specific_energy(orbit.ship) - e0) / std::fabs(e0);
    }

    double propagation_error(sl::FlightIntegrator integrator, double dt)
    {
        Orbit orbit(7.0e6, integrator, dt);
        orbit.world.step(1200.0);
        return orbit.position_error(1200.0);
    }

    void test_j2_gravity()
    {
        constexpr double reference = 6378136.3;
        constexpr double j2 = 1.08262668e-3;
        constexpr double r = reference + 400000.0;
        Orbit orbit(r, sl::FlightIntegrator::rk4, 20.0);
        sl::FlightBody *planet = orbit.world.body(orbit.planet);
        planet->gravity_j2 = j2;
        planet->gravity_reference_radius = reference;
        const double g = mu_earth / (r * r);
        const double correction = 1.5 * j2 * (reference / r) * (reference / r);
        const sl::dvec3 point(r, 0, 0);
        check(glm::length(orbit.world.gravity_at(point) - sl::dvec3(-g, 0, 0)) < 1e-12,
              "J2 disabled preserves point gravity");
        orbit.world.nonspherical_gravity = true;
        check(glm::length(orbit.world.gravity_at(point) - sl::dvec3(-g * (1.0 + correction), 0, 0)) < 1e-12,
              "J2 equatorial gravity");
        check(glm::length(orbit.world.gravity_at({0, r, 0}) - sl::dvec3(0, -g * (1.0 - 2.0 * correction), 0)) < 1e-12,
              "J2 polar gravity");
        planet->spin_axis = {1, 0, 0};
        check(glm::length(orbit.world.gravity_at(point) - sl::dvec3(-g * (1.0 - 2.0 * correction), 0, 0)) < 1e-12,
              "J2 follows the body's pole");
        planet->spin_axis = {0, 1, 0};

        const double inclination = 51.6 * pi / 180.0;
        const double v = std::sqrt(mu_earth / r);
        orbit.world.body(orbit.ship)->velocity = {0, v * std::sin(inclination), -v * std::cos(inclination)};
        const double initial_energy = orbit.world.specific_energy(orbit.ship);
        const auto node = [&]() {
            const sl::FlightBody *ship = orbit.world.body(orbit.ship);
            const sl::dvec3 h = glm::cross(ship->position, ship->velocity);
            return std::atan2(h.x, h.z);
        };
        const double initial = node();
        for (int i = 0; i < 1440; ++i)
            orbit.world.step(60.0);
        const double drift = std::remainder(node() - initial, 2.0 * pi);
        const double expected = -1.5 * std::sqrt(mu_earth / (r * r * r)) * j2 *
                                (reference / r) * (reference / r) * std::cos(inclination) * 86400.0;
        check(std::fabs(drift - expected) < std::fabs(expected) * 0.03,
              "J2 nodal drift agrees with secular theory within 3 percent");
        check(std::fabs(orbit.world.specific_energy(orbit.ship) - initial_energy) < std::fabs(initial_energy) * 1e-6,
              "J2 potential contributes to conserved specific energy");

        constexpr double mars_mu = 4.282837e13;
        constexpr double mars_reference = 3396000.0;
        constexpr double mars_j2 = 1.956608634e-3;
        constexpr double mars_r = mars_reference + 400000.0;
        Orbit mars(mars_r, sl::FlightIntegrator::rk4, 20.0);
        mars.world.body(mars.planet)->gm = mars_mu;
        mars.world.body(mars.planet)->gravity_j2 = mars_j2;
        mars.world.body(mars.planet)->gravity_reference_radius = mars_reference;
        mars.world.nonspherical_gravity = true;
        const double mars_v = std::sqrt(mars_mu / mars_r);
        mars.world.body(mars.ship)->velocity = {0, mars_v * std::sin(inclination), -mars_v * std::cos(inclination)};
        for (int i = 0; i < 1440; ++i)
            mars.world.step(60.0);
        const sl::FlightBody *mars_ship = mars.world.body(mars.ship);
        const sl::dvec3 mars_h = glm::cross(mars_ship->position, mars_ship->velocity);
        const double mars_drift = std::atan2(mars_h.x, mars_h.z);
        const double mars_expected = -1.5 * std::sqrt(mars_mu / (mars_r * mars_r * mars_r)) * mars_j2 *
                                     (mars_reference / mars_r) * (mars_reference / mars_r) * std::cos(inclination) * 86400.0;
        check(std::fabs(mars_drift - mars_expected) < std::fabs(mars_expected) * 0.03,
              "Mars J2 nodal drift agrees with secular theory within 3 percent");
    }

    void test_harmonic_gravity()
    {
        constexpr double reference = 6378136.3;
        constexpr double j2 = 1.08262668e-3;
        Orbit orbit(reference + 400000.0, sl::FlightIntegrator::rk4, 5.0);
        auto model = std::make_shared<sl::GravityHarmonics>();
        model->degree = 2;
        model->gm = mu_earth;
        model->reference_radius = reference;
        model->cosine.assign(6, 0.0);
        model->sine.assign(6, 0.0);
        model->cosine[3] = -j2 / std::sqrt(5.0);
        sl::FlightBody *planet = orbit.world.body(orbit.planet);
        planet->gravity_j2 = j2;
        planet->gravity_reference_radius = reference;
        orbit.world.nonspherical_gravity = true;
        const sl::dvec3 point(5e6, 3e6, -4e6);
        const sl::dvec3 j2_acceleration = orbit.world.gravity_at(point);
        planet->gravity_harmonics = model;
        check(glm::length(orbit.world.gravity_at(point) - j2_acceleration) < 1e-12,
              "normalized harmonic C20 matches analytic J2 without double counting");
        check(glm::length(orbit.world.gravity_at({0, reference + 400000.0, 0}) -
                          sl::dvec3(0, -mu_earth / std::pow(reference + 400000.0, 2) *
                                         (1.0 - 3.0 * j2 * std::pow(reference / (reference + 400000.0), 2)), 0)) < 1e-12,
              "harmonic field is finite and correct at pole");
        model->cosine[5] = 0.0001;
        model->sine[5] = 0.00007;
        orbit.world.body(orbit.ship)->position = point;
        const sl::dvec3 acceleration = orbit.world.gravity_at(point);
        for (int axis = 0; axis < 3; ++axis)
        {
            const double epsilon = 10.0;
            orbit.world.body(orbit.ship)->position = point;
            orbit.world.body(orbit.ship)->position[axis] += epsilon;
            const double plus = orbit.world.specific_energy(orbit.ship);
            orbit.world.body(orbit.ship)->position[axis] -= 2.0 * epsilon;
            const double minus = orbit.world.specific_energy(orbit.ship);
            check(std::fabs(acceleration[axis] + (plus - minus) / (2.0 * epsilon)) < 1e-8,
                  "harmonic acceleration matches potential gradient");
        }
        orbit.world.body(orbit.ship)->position = point;
        const sl::dvec3 before = orbit.world.gravity_at(point);
        planet->gravity_orientation = glm::angleAxis(pi / 2.0, sl::dvec3(0, 1, 0));
        check(glm::length(orbit.world.gravity_at(point) - before) > 1e-5,
              "tesseral harmonic rotates with the body");
        planet->gravity_orientation = sl::dquat(1, 0, 0, 0);
        planet->spin_rate = 0.01;
        orbit.world.step(20.0);
        const sl::dvec3 spun = orbit.world.gravity_at(point);
        planet->gravity_orientation = glm::angleAxis(0.2, sl::dvec3(0, 1, 0));
        planet->spin_rate = 0.0;
        check(glm::length(orbit.world.gravity_at(point) - spun) < 1e-12,
              "harmonic field follows elapsed spin time");
        planet->gravity_orientation = sl::dquat(1, 0, 0, 0);
        planet->gravity_frame = [](double time) { return glm::angleAxis(0.01 * time, sl::dvec3(0, 1, 0)); };
        check(glm::length(orbit.world.gravity_at(point) - spun) < 1e-12,
              "time-dependent gravity-frame callback is used");
        planet->gravity_frame = {};
        planet->gravity_orientation = sl::dquat(1, 0, 0, 0);
        orbit.world.nonspherical_gravity = false;
        check(glm::length(orbit.world.gravity_at(point) + mu_earth * point / std::pow(glm::length(point), 3)) < 1e-12,
              "disabled harmonic field preserves point gravity");

        model->cosine[5] = 0.02;
        Orbit coarse(reference + 400000.0, sl::FlightIntegrator::rk4, 1.0);
        Orbit fine(reference + 400000.0, sl::FlightIntegrator::rk4, 0.05);
        for (Orbit *run : {&coarse, &fine})
        {
            run->world.nonspherical_gravity = true;
            run->world.body(run->planet)->gravity_harmonics = model;
            run->world.body(run->planet)->gravity_frame = [](double time) {
                return glm::angleAxis(0.02 * time, sl::dvec3(0, 1, 0));
            };
            run->world.step(10.0);
        }
        check(glm::length(coarse.world.body(coarse.ship)->position - fine.world.body(fine.ship)->position) < 0.001,
              "RK stages sample the rotating harmonic field at their own times");
        auto invalid = std::make_shared<sl::GravityHarmonics>();
        coarse.world.body(coarse.planet)->gravity_harmonics = invalid;
        bool rejected = false;
        try
        {
            coarse.world.gravity_at(point);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        check(rejected, "malformed harmonic model is rejected");
    }

    void test_kepler()
    {
        const sl::dvec3 r(6.8e6, 1.2e6, -2.1e6);
        const sl::dvec3 v(2000.0, 5500.0, -3000.0);
        sl::OrbitElements el;
        check(sl::orbit_from_state(mu_earth, r, v, el), "elements from state");
        check(el.eccentricity > 0.0 && el.eccentricity < 1.0, "elliptic orbit");
        const sl::OrbitState back = sl::state_from_orbit(mu_earth, el);
        check(glm::length(back.position - r) < 1e-3 && glm::length(back.velocity - v) < 1e-6, "round trip");

        const sl::OrbitState later = sl::state_from_orbit(mu_earth, el, el.period(mu_earth));
        check(glm::length(later.position - r) < 1e-3, "period closes the orbit");

        for (const double eccentricity : {0.8, 0.95, 0.990797, 0.9999})
            for (int i = -500; i <= 500; ++i)
            {
                sl::OrbitElements input;
                input.semi_major_axis = 180.0e6;
                input.eccentricity = eccentricity;
                input.mean_anomaly = pi * i / 500.0;
                const sl::OrbitState state = sl::state_from_orbit(mu_earth, input);
                sl::OrbitElements recovered;
                const bool valid = sl::orbit_from_state(mu_earth, state.position, state.velocity, recovered);
                check(valid && std::abs(std::remainder(recovered.mean_anomaly - input.mean_anomaly, 2.0 * pi)) < 1e-9,
                      "high-eccentricity mean anomaly round trip");
            }

        sl::OrbitElements circ;
        check(sl::orbit_from_state(mu_earth, {7e6, 0, 0}, {0, 0, -std::sqrt(mu_earth / 7e6)}, circ), "circular");
        const sl::OrbitState circ_back = sl::state_from_orbit(mu_earth, circ);
        check(glm::length(circ_back.position - sl::dvec3(7e6, 0, 0)) < 1e-3, "circular round trip");
        check(std::fabs(circ.inclination) < 1e-9, "prograde equatorial inclination");

        sl::OrbitElements retro;
        check(sl::orbit_from_state(mu_earth, {7e6, 0, 0}, {0, 0, std::sqrt(mu_earth / 7e6)}, retro), "retrograde");
        check(std::fabs(retro.inclination - pi) < 1e-9, "retrograde inclination");

        const sl::dvec3 hr(8e6, 1e6, 0.0);
        const sl::dvec3 hv(1000.0, 3000.0, 11000.0);
        sl::OrbitElements hyp;
        check(sl::orbit_from_state(mu_earth, hr, hv, hyp), "hyperbolic");
        check(hyp.eccentricity > 1.0 && hyp.semi_major_axis < 0.0, "hyperbolic shape");
        const sl::OrbitState hyp_back = sl::state_from_orbit(mu_earth, hyp);
        check(glm::length(hyp_back.position - hr) < 1e-3 && glm::length(hyp_back.velocity - hv) < 1e-6,
              "hyperbolic round trip");
        check(hyp.period(mu_earth) == 0.0, "unbound orbit has no period");

        check(!sl::orbit_from_state(mu_earth, {7e6, 0, 0}, {-1000, 0, 0}, circ), "radial fall is degenerate");
        check(sl::orbit_path(mu_earth, el, 64).size() == 65, "orbit path samples");
    }

    void test_integrators()
    {
        using I = sl::FlightIntegrator;
        const double euler = energy_drift(I::euler, 20.0);
        const double semi = energy_drift(I::semi_implicit, 20.0);
        const double verlet = energy_drift(I::verlet, 20.0);
        const double rk4 = energy_drift(I::rk4, 20.0);
        const double rk6 = energy_drift(I::rk6, 20.0);
        std::cout << "energy drift euler " << euler << " semi " << semi << " verlet " << verlet << " rk4 " << rk4
                  << " rk6 " << rk6 << '\n';
        check(euler > semi, "semi-implicit beats Euler on energy");
        check(semi > rk4, "RK4 beats semi-implicit on energy");
        check(rk4 > rk6, "RK6 beats RK4 on energy");
        check(euler > verlet && verlet > rk4, "Verlet sits between Euler and RK4");

        struct Case
        {
            I integrator;
            double dt;
            double order;
        };
        const Case cases[] = {{I::euler, 6.0, 1.8}, {I::semi_implicit, 6.0, 1.8}, {I::verlet, 30.0, 3.5},
                              {I::rk4, 150.0, 12.0}, {I::rk6, 300.0, 40.0}};
        for (const Case &c : cases)
        {
            const double coarse = propagation_error(c.integrator, c.dt);
            const double fine = propagation_error(c.integrator, c.dt / 2.0);
            std::cout << "order ratio " << coarse / fine << " (coarse error " << coarse << " m)\n";
            check(fine > 0.0 && coarse / fine > c.order, "integrator convergence order");
        }
    }

    void test_rails()
    {
        Orbit dynamic(7.0e6, sl::FlightIntegrator::rk6, 5.0);
        Orbit rails(7.0e6, sl::FlightIntegrator::rk6, 5.0);
        check(rails.world.set_rails(rails.ship, rails.planet), "set rails");
        check(!rails.world.set_rails(rails.planet, rails.planet), "no rails around itself");
        dynamic.world.step(3000.0);
        rails.world.step(3000.0);
        check(glm::length(dynamic.world.body(dynamic.ship)->position - rails.world.body(rails.ship)->position) < 0.1,
              "rails matches integration");
        rails.world.step(1.0e6);
        check(std::fabs(glm::length(rails.world.body(rails.ship)->position) - 7.0e6) < 1.0, "rails has no drift");
        check(rails.world.release_rails(rails.ship), "release rails");

        sl::FlightWorld capped;
        const int p = capped.create_body("p");
        capped.body(p)->gm = mu_earth;
        capped.body(p)->mode = sl::FlightBodyMode::kinematic;
        const int s = capped.create_body("s");
        capped.body(s)->position = {7e6, 0, 0};
        capped.body(s)->velocity = {0, 0, -7546.0};
        capped.max_substeps_per_step = 50;
        capped.step(1.0e5);
        check(std::fabs(capped.time() - 1.0e5) < 1e-6, "substep cap still advances full time");
        check(std::isfinite(capped.body(s)->position.x), "capped step stays finite");
    }

    void test_rocket()
    {
        sl::FlightWorld world;
        const int id = world.create_body("rocket");
        sl::FlightBody *b = world.body(id);
        b->dry_mass = 1000.0;
        b->tanks.push_back({1000.0, 1000.0});
        sl::Thruster engine;
        engine.max_thrust = 10000.0;
        engine.isp = 300.0;
        engine.tank = 0;
        engine.level = 1.0;
        b->thrusters.push_back(engine);
        const double flow = 10000.0 / (300.0 * sl::standard_gravity);
        const double burn_time = 800.0 / flow;
        for (int i = 0; i < 100; ++i)
            world.step(burn_time / 100.0);
        const double expected = 300.0 * sl::standard_gravity * std::log(2000.0 / 1200.0);
        const double dv = -world.body(id)->velocity.z;
        check(std::fabs(dv - expected) / expected < 1e-6, "rocket equation delta-v");
        check(std::fabs(world.body(id)->tanks[0].mass - 200.0) < 1e-6, "propellant used");

        world.body(id)->tanks[0].mass = 0.0;
        const double before = world.body(id)->velocity.z;
        world.step(5.0);
        check(world.body(id)->velocity.z == before, "no thrust from an empty tank");
    }

    void test_rotation()
    {
        sl::FlightWorld world;
        const int id = world.create_body("tumbler");
        sl::FlightBody *b = world.body(id);
        b->dry_inertia = {1.0, 2.0, 3.0};
        b->angular_velocity = {1.0, 0.1, 0.5};
        world.integrator = sl::FlightIntegrator::rk6;
        world.max_substep = 0.05;
        auto energy = [&]
        { return 0.5 * glm::dot(b->angular_velocity, b->dry_inertia * b->angular_velocity); };
        auto momentum = [&]
        { return glm::length(b->orientation * (b->dry_inertia * b->angular_velocity)); };
        const double e0 = energy();
        const double l0 = momentum();
        world.step(100.0);
        check(std::fabs(energy() - e0) / e0 < 1e-6, "tumble conserves energy");
        check(std::fabs(momentum() - l0) / l0 < 1e-6, "tumble conserves angular momentum");
        check(std::fabs(glm::length(b->orientation) - 1.0) < 1e-12, "orientation stays normalised");
    }

    void test_thruster_groups()
    {
        sl::FlightWorld world;
        const int id = world.create_body("rcs");
        sl::FlightBody *b = world.body(id);
        b->dry_inertia = {100.0, 100.0, 100.0};
        sl::Thruster t;
        t.max_thrust = 100.0;
        t.direction = {0, 1, 0};
        t.position = {0, 0, -2};
        b->thrusters.push_back(t);
        t.direction = {0, -1, 0};
        t.position = {0, 0, 2};
        b->thrusters.push_back(t);
        world.build_standard_groups(id);
        bool found_pitch = false;
        for (const sl::ThrusterGroup &g : b->groups)
            if (g.name == "pitch_up" && g.thrusters.size() == 2)
                found_pitch = true;
        check(found_pitch, "pitch_up group built from layout");
        world.set_rcs_rotation(id, 1.0, 0.0, 0.0);
        check(b->thrusters[0].level == 1.0 && b->thrusters[1].level == 1.0, "pitch command fires both thrusters");
        world.step(1.0);
        check(b->angular_velocity.x > 0.0, "pitch up spins about +X");
        check(glm::length(b->velocity) < 1e-9, "couple adds no net force");
        world.set_rcs_rotation(id, 0.0, 0.0, 0.0);
        check(b->thrusters[0].level == 0.0, "zero command releases thrusters");
        check(world.set_group_level(id, "pitch_up", 0.5) && b->thrusters[1].level == 0.5, "named group level");
        check(!world.set_group_level(id, "nonsense", 1.0), "unknown group rejected");
        check(!world.add_group(id, "bad", {7}), "bad thruster index rejected");

        // A main and retro engine share translation groups with the RCS; RCS commands must not zero them.
        sl::FlightWorld ship_world;
        const int ship = ship_world.create_body("ship");
        sl::FlightBody *s = ship_world.body(ship);
        sl::Thruster main;
        main.max_thrust = 1000.0;
        main.direction = {0, 0, -1};
        s->thrusters.push_back(main);
        main.direction = {0, 0, 1};
        s->thrusters.push_back(main);
        ship_world.build_standard_groups(ship);
        ship_world.set_group_level(ship, "main", 0.8);
        ship_world.set_group_level(ship, "retro", 0.3);
        ship_world.set_rcs_rotation(ship, 1.0, 0.0, 0.0);
        ship_world.set_rcs_translation(ship, 0.0, 0.0, 0.0);
        check(s->thrusters[0].level == 0.8 && s->thrusters[1].level == 0.3, "RCS commands leave main and retro engines alone");
    }

    double wing_lift(double alpha)
    {
        sl::FlightWorld world;
        world.flat_atmosphere.surface_density = 1.225;
        const int id = world.create_body("wing");
        sl::FlightBody *b = world.body(id);
        b->dry_mass = 1000.0;
        b->velocity = 50.0 * sl::dvec3(0.0, -std::sin(alpha), -std::cos(alpha));
        sl::LiftingSurface wing;
        wing.area = 10.0;
        wing.cl_alpha = 5.0;
        b->surfaces.push_back(wing);
        const sl::dvec3 v0 = b->velocity;
        const double dt = 1e-4;
        world.max_substep = dt;
        world.step(dt);
        const sl::dvec3 force = (b->velocity - v0) / dt * b->mass();
        const sl::dvec3 lift_dir(0.0, std::cos(alpha), -std::sin(alpha));
        return glm::dot(force, lift_dir);
    }

    void test_aero()
    {
        const double lift = wing_lift(0.1);
        const double expected = 0.5 * 1.225 * 50.0 * 50.0 * 10.0 * 0.5;
        check(std::fabs(lift - expected) / expected < 0.01, "lift follows q S Cl");
        check(wing_lift(0.28) > wing_lift(0.7), "lift falls after the stall");
        check(wing_lift(-0.1) < 0.0, "negative angle gives negative lift");

        sl::FlightWorld world;
        world.flat_atmosphere.surface_density = 1.225;
        const int id = world.create_body("plane");
        sl::FlightBody *b = world.body(id);
        b->dry_mass = 1000.0;
        b->velocity = {0, 0, -50};
        sl::LiftingSurface elevator;
        elevator.control_name = "elevator";
        elevator.position = {0, 0, 5};
        elevator.area = 2.0;
        elevator.control_gain = 0.3;
        b->surfaces.push_back(elevator);
        b->dry_inertia = {1000.0, 1000.0, 1000.0};
        check(world.set_control(id, "elevator", 1.0) && !world.set_control(id, "missing", 1.0), "control lookup");
        world.step(0.5);
        check(std::fabs(b->angular_velocity.x) > 0.0, "elevator deflection produces a pitching moment");
        check(world.air_density_at({0, 0, 0}) > world.air_density_at({0, 10000, 0}), "density falls with height");
    }

    void test_gear()
    {
        sl::FlightWorld world;
        world.flat_ground = true;
        world.uniform_gravity = {0, -9.81, 0};
        world.max_substep = 0.01;
        const int id = world.create_body("plane");
        sl::FlightBody *b = world.body(id);
        b->dry_mass = 1000.0;
        b->dry_inertia = {1500.0, 2500.0, 2000.0};
        b->position = {0, 1.2, 0};
        b->velocity = {5.0, 0.0, -5.0};
        for (double x : {-1.0, 1.0})
            for (double z : {-2.0, 2.0})
            {
                sl::GearContact g;
                g.position = {x, -1.0, z};
                g.stiffness = 5.0e4;
                g.damping = 5.0e3;
                b->gear.push_back(g);
            }
        world.set_brakes(id, 1.0);
        for (int i = 0; i < 1000; ++i)
            world.step(0.01);
        check(std::fabs(b->position.y - (1.0 - 9810.0 / 2.0e5)) < 0.02, "settles on its springs");
        check(glm::length(b->velocity) < 0.1, "friction and brakes stop the vehicle");
        check(b->gear[0].compression > 0.0, "gear reports compression");
        check(b->position.y > 0.5, "never falls through the ground");
    }

    void test_prediction()
    {
        Orbit orbit(7.0e6, sl::FlightIntegrator::rk4, 1.0);
        const std::vector<sl::dvec3> path = orbit.world.predict_path(orbit.ship, 600.0, 6);
        check(path.size() == 7, "prediction samples");
        orbit.world.step(600.0);
        check(glm::length(path.back() - orbit.world.body(orbit.ship)->position) < 1.0, "prediction matches stepping");
        check(orbit.world.time() == 600.0, "prediction leaves the world untouched");

        orbit.world.apply_force(orbit.ship, {0, 0, 0});
        check(orbit.world.body(99) == nullptr, "unknown body is null");
        check(!orbit.world.destroy_body(99), "destroy unknown body");
        sl::FlightWorld empty;
        empty.step(-1.0);
        empty.step(std::nan(""));
        check(empty.time() == 0.0, "bad time steps are ignored");
    }
}

void test_isp_pressure()
{
    sl::FlightBody body;
    body.tanks.push_back({1000.0, 1000.0});
    sl::Thruster t;
    t.max_thrust = 1000.0;
    t.isp = 400.0;
    t.isp_sea_level = 300.0;
    t.tank = 0;
    t.level = 1.0;
    body.thrusters.push_back(t);
    check(std::fabs(sl::thruster_output(body, t, 0.0) - 1000.0) < 1e-9, "vacuum thrust is the rated thrust");
    check(std::fabs(sl::thruster_output(body, t, 1.225) - 750.0) < 1e-6, "sea-level thrust follows the sea-level Isp");
    sl::Thruster flat = t;
    flat.isp_sea_level = 0.0;
    check(std::fabs(sl::thruster_output(body, flat, 1.225) - 1000.0) < 1e-9, "no sea-level Isp leaves thrust alone");
}

int main()
{
    test_kepler();
    test_j2_gravity();
    test_harmonic_gravity();
    test_integrators();
    test_rails();
    test_rocket();
    test_rotation();
    test_thruster_groups();
    test_aero();
    test_gear();
    test_prediction();
    test_isp_pressure();
    if (failures)
    {
        std::cerr << failures << " flight test(s) failed.\n";
        return 1;
    }
    return 0;
}
