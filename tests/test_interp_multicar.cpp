#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <f1tm/interp.hpp>

using Catch::Approx;
using namespace f1tm;

TEST_CASE("InterpBuffer interpolates multiple cars by sim_time") {
  InterpBuffer ib;
  SimSnapshot a{}, b{};
  a.sim_time = 0.0; a.tick = 1;
  b.sim_time = 1.0; b.tick = 2;

  CarPose a0{0, 0.0, 0.0, 0.0, 0.0, 0};
  CarPose b0{0, 10.0, 20.0, std::numbers::pi_v<double>/2.0, 30.0, 0};
  CarPose a1{1, 5.0,  5.0,  0.0,  1.0,  0};
  CarPose b1{1, 15.0, 25.0, std::numbers::pi_v<double>/2.0, 31.0, 0};
  a.cars = {a0, a1};
  b.cars = {b0, b1};

  ib.push(a);
  ib.push(b);

  SimSnapshot out{};
  REQUIRE(ib.sample(0.5, out));
  REQUIRE(out.cars.size() == 2);
  const auto c0 = find_car(out, 0).value();
  const auto c1 = find_car(out, 1).value();

  REQUIRE(c0.x == Approx(5.0));
  REQUIRE(c0.y == Approx(10.0));
  REQUIRE(c0.s == Approx(15.0));
  REQUIRE(c0.heading_rad == Approx(std::numbers::pi_v<double>/4.0).margin(1e-9));

  REQUIRE(c1.x == Approx(10.0));
  REQUIRE(c1.y == Approx(15.0));
  REQUIRE(c1.s == Approx(16.0));
  REQUIRE(c1.heading_rad == Approx(std::numbers::pi_v<double>/4.0).margin(1e-9));
}

TEST_CASE("InterpBuffer clamps when car missing on one side") {
  InterpBuffer ib;
  SimSnapshot a{}, b{};
  a.sim_time = 0.0;
  b.sim_time = 1.0;

  // car 42 only in A, car 7 only in B
  a.cars.push_back(CarPose{42, 1.0, 2.0, 0.1,  5.0, 0});
  b.cars.push_back(CarPose{ 7, 9.0, 8.0, 0.2, 15.0, 0});

  ib.push(a); ib.push(b);

  SimSnapshot o{};
  REQUIRE(ib.sample(0.5, o));
  // both should be present, clamped
  auto c42 = find_car(o, 42).value();
  auto c7  = find_car(o, 7 ).value();
  REQUIRE(c42.x == Approx(1.0));
  REQUIRE(c7.x  == Approx(9.0));
}
