#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

#include <f1tm/snap.hpp>
#include <f1tm/interp.hpp>

using Catch::Approx;
using namespace f1tm;

static constexpr double PI = 3.14159265358979323846;
static constexpr double TAU = 2.0 * PI;

TEST_CASE("InterpBuffer linear interpolation by sim_time") {
  InterpBuffer ib;
  SimSnapshot a{}, b{};

  a.sim_time = 0.0; a.x = 0.0; a.y = 0.0; a.s = 0.0; a.heading_rad = 0.0; a.lap = 0; a.tick = 1;
  b.sim_time = 1.0; b.x = 10.0; b.y = 20.0; b.s = 30.0; b.heading_rad = PI/2; b.lap = 0; b.tick = 2;

  ib.push(a);
  ib.push(b);

  SimSnapshot out{};
  REQUIRE(ib.sample(0.5, out));  // half-way
  REQUIRE(out.x == Approx(5.0));
  REQUIRE(out.y == Approx(10.0));
  REQUIRE(out.s == Approx(15.0));
  REQUIRE(out.heading_rad == Approx(PI/4).margin(1e-9)); // 45 deg
  REQUIRE(out.lap == 0); // floor to earlier snapshot
  REQUIRE(out.tick >= 1);
}

TEST_CASE("InterpBuffer clamps outside range") {
  InterpBuffer ib;
  SimSnapshot a{}, b{};
  a.sim_time = 2.0; a.x = 2.0;
  b.sim_time = 3.0; b.x = 4.0;
  ib.push(a); ib.push(b);

  SimSnapshot out{};
  // before oldest -> clamps to oldest
  REQUIRE(ib.sample(1.5, out));
  REQUIRE(out.x == Approx(2.0));
  // after newest -> clamps to newest
  REQUIRE(ib.sample(3.5, out));
  REQUIRE(out.x == Approx(4.0));
}

TEST_CASE("InterpBuffer shortest-angle wrap around 2pi") {
  InterpBuffer ib;
  SimSnapshot a{}, b{}, out{};

  // a: 359 deg, b: 1 deg -> shortest path is +2 deg across wrap
  a.sim_time = 0.0; a.heading_rad = TAU - (PI/180.0);
  b.sim_time = 1.0; b.heading_rad = (PI/180.0);
  ib.push(a); ib.push(b);

  REQUIRE(ib.sample(0.5, out));
  // halfway should be ~0 deg (cos ~ 1, sin ~ 0)
  REQUIRE(std::cos(out.heading_rad) == Approx(1.0).margin(1e-9));
  REQUIRE(std::sin(out.heading_rad) == Approx(0.0).margin(1e-9));
}

TEST_CASE("InterpBuffer requires at least one snapshot") {
  InterpBuffer ib;
  SimSnapshot out{};
  REQUIRE_FALSE(ib.sample(0.0, out)); // empty buffer
  // one sample -> returns that one regardless of time
  SimSnapshot a{}; a.sim_time = 42.0; a.x = 7.0;
  ib.push(a);
  REQUIRE(ib.sample(0.0, out));
  REQUIRE(out.x == Approx(7.0));
}
