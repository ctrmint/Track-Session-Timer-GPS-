#include "track_timer/track/projection.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

void assert_near(const double actual, const double expected,
                 const double tolerance = 0.001)
{
    assert(std::abs(actual - expected) <= tolerance);
}

}  // namespace

int main()
{
    using namespace track_timer::track;

    CircuitProjection equator{};
    assert(configure_circuit_projection({0.0, 0.0}, equator) ==
           ProjectionResult::projected);
    LocalPoint local{99.0, 99.0};
    assert(project_to_circuit_local(equator, {0.0, 0.0}, local) ==
           ProjectionResult::projected);
    assert(local.east_m == 0.0);
    assert(local.north_m == 0.0);

    assert(project_to_circuit_local(equator, {0.1, 0.0}, local) ==
           ProjectionResult::projected);
    assert_near(local.east_m, 0.0);
    assert_near(local.north_m, 11'057.427582);
    assert(project_to_circuit_local(equator, {0.0, 0.1}, local) ==
           ProjectionResult::projected);
    assert_near(local.east_m, 11'131.949079);
    assert_near(local.north_m, 0.0);

    CircuitProjection northern{};
    assert(configure_circuit_projection({52.0, -1.0}, northern) ==
           ProjectionResult::projected);
    assert(project_to_circuit_local(northern, {52.001, -0.999}, local) ==
           ProjectionResult::projected);
    assert_near(local.east_m, 68.678016);
    assert_near(local.north_m, 111.267353);

    CircuitProjection southern{};
    assert(configure_circuit_projection({-33.9, 151.2}, southern) ==
           ProjectionResult::projected);
    assert(project_to_circuit_local(southern, {-33.899, 151.199}, local) ==
           ProjectionResult::projected);
    assert_near(local.east_m, -92.492903);
    assert_near(local.north_m, 110.920581);

    CircuitProjection east_dateline{};
    assert(configure_circuit_projection({0.0, 179.999}, east_dateline) ==
           ProjectionResult::projected);
    assert(project_to_circuit_local(east_dateline, {0.0, -179.999}, local) ==
           ProjectionResult::projected);
    assert_near(local.east_m, 222.638982);
    CircuitProjection west_dateline{};
    assert(configure_circuit_projection({0.0, -179.999}, west_dateline) ==
           ProjectionResult::projected);
    assert(project_to_circuit_local(west_dateline, {0.0, 179.999}, local) ==
           ProjectionResult::projected);
    assert_near(local.east_m, -222.638982);

    const LocalPoint sentinel{12.0, 34.0};
    local = sentinel;
    const CircuitProjection unconfigured{};
    assert(project_to_circuit_local(unconfigured, {0.0, 0.0}, local) ==
           ProjectionResult::unconfigured);
    assert(local.east_m == sentinel.east_m && local.north_m == sentinel.north_m);
    assert(project_to_circuit_local(equator, {91.0, 0.0}, local) ==
           ProjectionResult::invalid_point);
    assert(local.east_m == sentinel.east_m && local.north_m == sentinel.north_m);
    assert(project_to_circuit_local(equator, {1.0, 0.0}, local) ==
           ProjectionResult::outside_supported_radius);
    assert(local.east_m == sentinel.east_m && local.north_m == sentinel.north_m);

    const auto active_projection = northern;
    assert(configure_circuit_projection(
               {kMaximumCircuitProjectionReferenceLatitudeDeg + 0.001, 0.0}, northern) ==
           ProjectionResult::invalid_reference);
    assert(northern.reference.latitude_deg == active_projection.reference.latitude_deg);
    assert(configure_circuit_projection(
               {std::numeric_limits<double>::quiet_NaN(), 0.0}, northern) ==
           ProjectionResult::invalid_reference);
    CircuitProjection high_latitude{};
    assert(configure_circuit_projection(
               {kMaximumCircuitProjectionReferenceLatitudeDeg, 0.0}, high_latitude) ==
           ProjectionResult::projected);

    for (const auto result : {ProjectionResult::projected, ProjectionResult::unconfigured,
                              ProjectionResult::invalid_reference,
                              ProjectionResult::invalid_point,
                              ProjectionResult::outside_supported_radius}) {
        assert(projection_result_name(result)[0] != '\0');
    }

    std::cout << "WGS84 circuit-local projection reference vectors passed\n";
    return 0;
}
