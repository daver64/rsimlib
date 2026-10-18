# Flight & Orbital Dynamics

[← Back to README](../README.md)

`flight.h` provides `sl::FlightWorld`, a double-precision 3D rigid-body simulation aimed at Orbiter-style spaceflight and aircraft flight simulators. It is independent of the Box2D wrapper in [physics.md](physics.md).

Examples: `exorbit` (spaceflight) and `exflight` (aircraft). Test: `flight_test`.

---

## Conventions

- Right-handed, Y up. Body frame: +X right, +Y up, -Z forward. Pitch-up is positive about X, yaw-left about Y, roll-left about Z.
- SI units throughout (metres, kilograms, newtons, seconds, radians). Everything is `double` (`dvec3`, `dquat`); convert to `float` only for rendering.
- Orbit reference plane is XZ with +Y as the pole and +X as the zero-longitude direction.

---

## Bodies

`FlightWorld::create_body(name)` returns an id; `body(id)` returns a `FlightBody*` (invalidated by the next `create_body`). Each body has a `mode`:

| Mode | Behaviour |
| --- | --- |
| `dynamic` | Integrated under gravity, thrust, aerodynamics, gear and user forces. |
| `kinematic` | Constant velocity and angular velocity; use for planets that never move. |
| `rails` | Follows an analytic Kepler orbit around a parent (`set_rails(id, parent)`); no drift, ideal for planets, moons and time warp. `release_rails(id)` returns it to dynamic. |

Bodies with `gm > 0` attract every other body. A body with `radius > 0` is a ground surface and the base of its `atmosphere` (`surface_density`, `scale_height`), and `spin_rate` drives the air and ground velocity.

Mass is `dry_mass` plus propellant in `tanks`; `dry_inertia` scales with the current mass.

---

## Forces and vehicle parts

- **Thrusters** (`Thruster`): position, direction, `max_thrust`, `isp`, `tank` and `level` (0-1). Propellant use is `thrust / (isp * g0)`; an empty tank gives no thrust. `air_breathing` thrusters fade with air density.
- **Thruster groups**: `add_group`, `set_group_level`, and `build_standard_groups(id)`, which builds `main`, `retro`, `hover`, translation groups (`x+`, `x-`, `y+`, `y-`, `z+`, `z-`) and rotation groups (`pitch_up`, `pitch_down`, `yaw_left`, `yaw_right`, `roll_left`, `roll_right`). `set_rcs_rotation` and `set_rcs_translation` drive them with -1..1 inputs; thrusters in the `main` and `retro` groups are never changed by RCS commands, so engine throttle and RCS can be used together.
- **Lifting surfaces** (`LiftingSurface`): linear lift slope to a stall angle then a flat-plate blend, with `cl0`, `cd0` and induced drag. `control_name` and `control_gain` make a surface a control surface; set it with `set_control(id, name, value)`. Stalls, trim and control authority emerge from the coefficients.
- **Landing gear** (`GearContact`): spring/damper against flat ground or a planet surface, with lateral friction, rolling resistance and brakes (`set_brakes`). `compression` reports the last step.
- **Parasitic drag**: `drag_area` (Cd * A) and `angular_damping`.
- **User forces**: `apply_force`, `apply_force_at`, `apply_torque` apply to the next step only.

World settings: `uniform_gravity`, `flat_ground`/`flat_ground_height`, `flat_atmosphere` and `wind`.

---

## Integration and time warp

`FlightWorld::integrator` selects `euler`, `semi_implicit`, `verlet`, `rk4` (default) or `rk6`. `step(dt)` divides `dt` into equal substeps no larger than `max_substep`, capped at `max_substeps_per_step` (the substep grows instead), so very large time warps cannot stall a frame. For long warps put celestial bodies on rails.

---

## Orbits

```cpp
sl::OrbitElements el;
if (sl::orbit_from_state(mu, position, velocity, el)) {
    double period = el.period(mu);
    double peri = el.periapsis();
}
sl::OrbitState s = sl::state_from_orbit(mu, el, /*dt*/ 60.0);
std::vector<sl::dvec3> path = sl::orbit_path(mu, el, 256);
```

World helpers: `orbit_of(id, parent, out)`, `relative_position`, `specific_energy`, `gravity_at`, `altitude_of`, `air_density_at`, `air_velocity_at`, and `predict_path(id, duration, samples)`, which simulates a copy of the world for drawing trajectory predictions.

---

## Rendering with a floating origin

At planetary scales `float` positions lose precision. Draw everything relative to the vessel:

```cpp
glm::mat4 model = world.render_matrix(id, vessel_position);
```

`render_matrix` returns a float model matrix with the translation taken relative to `origin` (computed in double precision).

---

## Minimal example

```cpp
sl::FlightWorld world;
int earth = world.create_body("earth");
sl::FlightBody &e = *world.body(earth);
e.mode = sl::FlightBodyMode::kinematic;
e.gm = 3.986004418e14;
e.radius = 6.371e6;

int ship = world.create_body("ship");
sl::FlightBody &s = *world.body(ship);
s.position = {0.0, 0.0, 6.771e6};
s.velocity = {7670.0, 0.0, 0.0};

while (running)
    world.step(dt);
```

---

[← Back to README](../README.md)
