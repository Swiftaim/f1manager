#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <string>
#include <random>

#include <f1tm/events.hpp>
#include <f1tm/track.hpp>
#include <f1tm/race.hpp>

using namespace f1tm;

TEST_CASE("simulate_pit_events is deterministic with same seed") {
  std::mt19937 r1(42), r2(42);
  auto e1 = simulate_pit_events(8, 0.3, 0.2, r1);
  auto e2 = simulate_pit_events(8, 0.3, 0.2, r2);
  REQUIRE(e1 == e2);
}

TEST_CASE("simulate_pit_events changes with different seeds") {
  std::mt19937 r1(42), r2(43);
  auto e1 = simulate_pit_events(8, 0.3, 0.2, r1);
  auto e2 = simulate_pit_events(8, 0.3, 0.2, r2);
  REQUIRE(e1 != e2); // highly likely, acceptable for micro tests
}

TEST_CASE("simulate_pit_events handles edges") {
  std::mt19937 rng(7);
  auto all_sc    = simulate_pit_events(5, 1.0, 0.0, rng);
  auto all_vsc   = simulate_pit_events(5, 0.0, 1.0, rng);
  auto all_green = simulate_pit_events(5, 0.0, 0.0, rng);

  for (auto& s : all_sc)    REQUIRE(s == "SC");
  for (auto& s : all_vsc)   REQUIRE(s == "VSC");
  for (auto& s : all_green) REQUIRE(s == "GREEN");
}

TEST_CASE("simulate_lane_factors matches mapping via lane_factors_from_events") {
  auto t = track_by_key("Bahrain");
  REQUIRE(t.has_value());

  std::mt19937 rng(123);
  const size_t n = 6;
  auto events  = simulate_pit_events(n, 0.25, 0.25, rng);
  auto f1      = lane_factors_from_events(*t, events);
  // Same seed, same parameters -> simulate_lane_factors should match
  std::mt19937 rng2(123);
  auto f2      = simulate_lane_factors(n, *t, 0.25, 0.25, rng2);
  REQUIRE(f1 == f2);
}
