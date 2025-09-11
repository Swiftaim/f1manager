#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <f1tm/sim.hpp>

using Catch::Approx;
using namespace f1tm;

TEST_CASE("Time warp scales advancement linearly") {
  SimServer sim;
  sim.track.radius_m = 100.0;
  sim.car.speed_mps  = 10.0;

  const double base_dt = 0.5;    // seconds
  const double scale1  = 1.0;
  const double scale2  = 2.0;

  // advance with scale 1
  SimServer a = sim;
  a.step(base_dt * scale1);
  const double s1 = a.car.s;

  // advance with scale 2
  SimServer b = sim;
  b.step(base_dt * scale2);
  const double s2 = b.car.s;

  REQUIRE(s2 == Approx(2.0 * s1));
}

TEST_CASE("Pause (scale 0) yields no advancement") {
  SimServer sim;
  sim.car.speed_mps = 50.0;
  const double s0 = sim.car.s;
  sim.step(0.0); // paused
  REQUIRE(sim.car.s == Approx(s0));
}
