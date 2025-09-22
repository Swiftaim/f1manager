#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <f1tm/sim.hpp>

using Catch::Approx;
using namespace f1tm;

TEST_CASE("Time warp scales advancement linearly") {
  SimServer sim;
  sim.track.radius_m = 100.0;
  sim.add_car(0, 10.0);

  const double base_dt = 0.5;    // seconds
  const double scale1  = 1.0;
  const double scale2  = 2.0;

  // advance with scale 1
  SimServer a = sim;
  a.step(base_dt * scale1);
  const auto* car1 = a.car_by_index(0);
  REQUIRE(car1 != nullptr);
  const double s1 = car1->s;

  // advance with scale 2
  SimServer b = sim;
  b.step(base_dt * scale2);
  const auto* car2 = b.car_by_index(0);
  REQUIRE(car2 != nullptr);
  const double s2 = car2->s;

  REQUIRE(s2 == Approx(2.0 * s1));
}

TEST_CASE("Pause (scale 0) yields no advancement") {
  SimServer sim;
  sim.add_car(0, 50.0);
  auto* car0 = sim.car_by_index(0);
  REQUIRE(car0 != nullptr);
  const double s0 = car0->s;
  sim.step(0.0); // paused
  REQUIRE(car0->s == Approx(s0));
}
