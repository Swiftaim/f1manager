#include <catch2/catch_test_macros.hpp>
#include <f1tm/sim.hpp>

using namespace f1tm;

TEST_CASE("SimServer advances along a circle and wraps laps") {
  SimServer sim;
  sim.track.radius_m = 10.0;          // circumference ≈ 62.8319 m
  sim.car.speed_mps = 10.0;           // 10 m/s
  const double C = sim.track.circumference_m();
  REQUIRE(C > 0.0);

  // Step exactly one lap worth of time
  const double lap_time = C / sim.car.speed_mps;
  double dt = lap_time / 10.0;        // 10 fixed steps
  for (int i = 0; i < 10; ++i) sim.step(dt);

  // We should be back near s ≈ 0 with 1 lap completed
  REQUIRE(sim.car.laps == 1);
  REQUIRE(sim.car.s >= 0.0);
  REQUIRE(sim.car.s < C);
}
