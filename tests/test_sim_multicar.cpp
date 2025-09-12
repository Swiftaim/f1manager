#include <catch2/catch_test_macros.hpp>
#include <f1tm/sim.hpp>

using namespace f1tm;

TEST_CASE("SimServer advances multiple cars and preserves ids") {
  SimServer sim;
  sim.track.radius_m = 10.0;               // C ≈ 62.8319 m

  // Add three cars with different speeds and offsets
  sim.add_car(7, 10.0, 0.0);               // id=7
  sim.add_car(9,  5.0, 1.0);               // id=9
  sim.add_car(3, 20.0, 2.0);               // id=3
  REQUIRE(sim.car_count() == 3);

  // Step 1 second
  sim.step(1.0);

  const auto* c0 = sim.car_by_index(0);
  const auto* c1 = sim.car_by_index(1);
  const auto* c2 = sim.car_by_index(2);
  REQUIRE(c0); REQUIRE(c1); REQUIRE(c2);

  REQUIRE(c0->id == 7);
  REQUIRE(c1->id == 9);
  REQUIRE(c2->id == 3);

  // s increased by speed * dt
  REQUIRE(c0->s > 9.9);
  REQUIRE(c1->s > 5.9);
  REQUIRE(c2->s > 21.9);

  // Now step enough to wrap at least once for fastest car
  const double C = sim.track.circumference_m();
  const double t_one_lap_fastest = (C - c2->s) / c2->speed_mps + 0.01; // small margin
  sim.step(t_one_lap_fastest);

  // Fastest car should have at least 1 lap
  REQUIRE(sim.car_by_index(2)->laps >= 1);
}

TEST_CASE("sample_pose_index returns a valid pose per car") {
  SimServer sim;
  sim.track.radius_m = 15.0;
  sim.add_car(1, 15.0, 0.0);
  sim.add_car(2, 15.0, 10.0);

  double x1,y1,h1, x2,y2,h2;
  sim.sample_pose_index(0, x1,y1,h1);
  sim.sample_pose_index(1, x2,y2,h2);

  // Poses should differ given different s
  REQUIRE( (x1 != x2 || y1 != y2) );
}
