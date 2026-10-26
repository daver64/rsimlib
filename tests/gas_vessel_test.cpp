#include "gas_vessel.h"

#include <cassert>
#include <cmath>
#include <cstdio>

int main()
{
    using namespace sl;
    GasVessel *v = create_gas_vessel(12);
    assert(v && gas_vessel_open_cells(v) == 1000);

    // Mixing conserves mass and equalises a point source.
    gas_vessel_inject(v, 2, 2, 2, VesselGas::Oxygen, 10.0f);
    gas_vessel_step(v, 60.0f);
    assert(std::fabs(gas_vessel_total(v, VesselGas::Oxygen) - 10.0f) < 1e-3f);
    const float lo = gas_vessel_cell(v, 2, 2, 2, VesselGas::Oxygen);
    const float far = gas_vessel_cell(v, 9, 9, 9, VesselGas::Oxygen);
    assert(far > 0.0f && lo > far);
    gas_vessel_step(v, 3000.0f);
    assert(std::fabs(gas_vessel_cell(v, 2, 2, 2, VesselGas::Oxygen) - 0.01f) < 0.002f);

    // Consume removes only what exists.
    assert(gas_vessel_consume(v, 5, 5, 5, VesselGas::Oxygen, 100.0f) < 0.02f);

    // A vent drains gas to vacuum and reports the mass lost.
    gas_vessel_fill(v, VesselGas::Nitrogen, 50.0f);
    gas_vessel_set_vent(v, 0, 5, 5, 0.5f);
    const float before = gas_vessel_total(v, VesselGas::Nitrogen);
    const float vented = gas_vessel_step(v, 1200.0f);
    const float after = gas_vessel_total(v, VesselGas::Nitrogen);
    assert(vented > 0.0f);
    assert(after < before * 0.9f);
    assert(before - after <= vented + 1e-3f); // vented also counts oxygen

    gas_vessel_set_total(v, VesselGas::Nitrogen, 5.0f);
    assert(std::fabs(gas_vessel_total(v, VesselGas::Nitrogen) - 5.0f) < 1e-3f);

    destroy_gas_vessel(v);
    std::puts("gas_vessel_test ok");
    return 0;
}
