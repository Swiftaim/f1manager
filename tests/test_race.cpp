#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <optional>
#include <vector>

#include <f1tm/stint.hpp>
#include <f1tm/pit.hpp>
#include <f1tm/race.hpp>

using Catch::Approx;
using namespace f1tm;

TEST_CASE("race_time: sum of pure stints") {
  std::vector<StintParams> stints{
    {3, 90.0, 0.2},  // time = 3*90 + 0.2*3*2/2 = 270 + 0.6 = 270.6
    {2, 91.0, 0.1}   // time = 2*91 + 0.1*2*1/2 = 182 + 0.1 = 182.1
  };
  auto total = race_time(stints);
  REQUIRE(total.has_value());
  REQUIRE(*total == Approx(270.6 + 182.1));
}

TEST_CASE("race_time_with_pits: adds pit loss between stints") {
  std::vector<StintParams> stints{
    {3, 90.0, 0.2},  // 270.6
    {2, 91.0, 0.1}   // 182.1
  };
  std::vector<PitParams> pits{
    {2.5, 17.0}      // 19.5
  };
  auto total = race_time_with_pits(stints, pits);
  REQUIRE(total.has_value());
  REQUIRE(*total == Approx(270.6 + 19.5 + 182.1));
}

TEST_CASE("race_time_with_pits_under: SC/VSC lane factors reduce loss") {
  std::vector<StintParams> stints{
    {3, 90.0, 0.2},  // 270.6
    {2, 91.0, 0.1}   // 182.1
  };
  std::vector<PitParams> pits{
    {2.5, 17.0}      // base loss 19.5
  };
  // Under SC lane_factor=0.5 -> 2.5 + 17*0.5 = 11.0
  std::vector<double> lane_factors{0.5};

  auto total = race_time_with_pits_under(stints, pits, lane_factors);
  REQUIRE(total.has_value());
  REQUIRE(*total == Approx(270.6 + 11.0 + 182.1));
}

TEST_CASE("race_time: invalid sizes are rejected") {
  std::vector<StintParams> stints{{2, 90.0, 0.0}};
  std::vector<PitParams> pits{{2.0, 10.0}, {2.0, 10.0}};
  REQUIRE_FALSE(race_time_with_pits(stints, pits).has_value());
  REQUIRE_FALSE(race_time_with_pits_under(stints, pits, {0.5, 0.5}).has_value());
}
