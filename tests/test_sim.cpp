#include <catch2/catch_test_macros.hpp>
#include <f1tm/sim.hpp>

using namespace f1tm;

TEST_CASE("SimServer advances along a circle and wraps laps (single car via add_car)") {
  SimServer sim;
  sim.track.radius_m = 10.0;          // circumference ≈ 62.8319 m
  sim.add_car(0, 10.0, 0.0);          // id=0, 10 m/s

  const double C = sim.track.circumference_m();
  REQUIRE(C > 0.0);

  // Step exactly one lap worth of time
  const double lap_time = C / 10.0;
  #ifdef _MSC_VER
    (void)lap_time;
  #endif
  double dt = (C / 10.0) / 10.0;      // 10 fixed steps
  for (int i = 0; i < 10; ++i) sim.step(dt);

  const auto* car0 = sim.car_by_index(0);
  REQUIRE(car0 != nullptr);
  REQUIRE(car0->laps == 1);
  REQUIRE(car0->s >= 0.0);
  REQUIRE(car0->s < C);
}
