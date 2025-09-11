#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <optional>
#include <vector>
#include <string>

#include <f1tm/stint.hpp>
#include <f1tm/pit.hpp>
#include <f1tm/track.hpp>
#include <f1tm/race.hpp>

using Catch::Approx;
using namespace f1tm;

TEST_CASE("lane_factors_from_events maps events via Track") {
  auto t = track_by_key("Bahrain");
  REQUIRE(t.has_value());

  std::vector<std::string> events = {"SC", "VSC", "GREEN"};
  auto factors = lane_factors_from_events(*t, events);
  REQUIRE(factors.size() == 3);
  REQUIRE(factors[0] == Approx(t->sc_lane_factor));
  REQUIRE(factors[1] == Approx(t->vsc_lane_factor));
  REQUIRE(factors[2] == Approx(1.0));
}

TEST_CASE("race_time_with_track composes stints and track pits with events") {
  // Two-stint race with one pit; event is SC at that pit.
  std::vector<StintParams> stints{
    {3, 90.0, 0.2},  // 270.6
    {2, 91.0, 0.1}   // 182.1
  };

  auto t = track_by_key("Bahrain");
  REQUIRE(t.has_value());
  // Bahrain from our built-in catalog: stationary=2.5, lane=17.0, sc_factor=0.45
  // Pit under SC: 2.5 + 17.0 * 0.45 = 10.15
  std::vector<std::string> events = {"SC"};

  auto total = race_time_with_track(stints, *t, events);
  REQUIRE(total.has_value());
  REQUIRE(*total == Approx(270.6 + 10.15 + 182.1));
}

TEST_CASE("race_time_with_track validates sizes and event names") {
  std::vector<StintParams> stints{{2, 90.0, 0.0}};
  auto t = track_by_key("Bahrain");
  REQUIRE(t.has_value());

  // Wrong count (need stints.size()-1 events)
  REQUIRE_FALSE(race_time_with_track(stints, *t, {"SC","VSC"}).has_value());

  // Unknown event should fail (strict)
  REQUIRE_FALSE(race_time_with_track({{2,90,0},{2,90,0}}, *t, {"UNKNOWN"}).has_value());
}
