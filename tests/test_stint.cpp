#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <f1tm/stint.hpp>

using namespace f1tm;
using Catch::Approx;

TEST_CASE("estimate_stint_time") {
  SECTION("linear degradation over n laps") {
    StintParams p{5, 90.0, 0.2}; // 90.0 + 90.2 + ... + 90.8 = 451.0
    REQUIRE(estimate_stint_time(p) == Approx(452.0));
  }

  SECTION("zero laps yields zero time") {
    StintParams p{0, 90.0, 0.3};
    REQUIRE(estimate_stint_time(p) == Approx(0.0));
  }

  SECTION("defensive input: negative values clamp to zero") {
    StintParams p{-3, -10.0, -1.0};
    REQUIRE(estimate_stint_time(p) == Approx(0.0));
  }

  SECTION("no degradation works (constant pace)") {
    StintParams p{4, 88.5, 0.0}; // 4 * 88.5 = 354.0
    REQUIRE(estimate_stint_time(p) == Approx(354.0));
  }
}
