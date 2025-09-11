#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <sstream>
#include <optional>
#include <vector>

#include <f1tm/track.hpp>
#include <f1tm/pit.hpp>

using Catch::Approx;
using namespace f1tm;

static std::string csv_minimal = R"(key,pit_stationary_s,pit_lane_delta_s,sc_lane_factor,vsc_lane_factor
Bahrain,2.6,16.5,0.50,0.80
Monaco,2.5,21.5,0.40,0.70
)";

static std::string csv_with_noise = R"( key , pit_stationary_s , pit_lane_delta_s , sc_lane_factor , vsc_lane_factor
# comment lines are ignored
Bahrain , 2.6 , 16.5 , 0.50 , 0.80
, , , ,            # bad row skipped
Monaco, 2.5 , 21.5 , 0.40 , 0.70
)";

TEST_CASE("track_catalog_from_csv_stream parses valid rows") {
  std::istringstream ss(csv_minimal);
  auto cat = track_catalog_from_csv_stream(ss);
  REQUIRE(cat.size() == 2);

  auto bah = track_by_key_in(cat, "Bahrain");
  REQUIRE(bah.has_value());
  REQUIRE(bah->pit_stationary_s == Approx(2.6));
  REQUIRE(bah->pit_lane_delta_s == Approx(16.5));
  REQUIRE(bah->sc_lane_factor == Approx(0.50));
  REQUIRE(bah->vsc_lane_factor == Approx(0.80));

  auto mon = track_by_key_in(cat, "Monaco");
  REQUIRE(mon.has_value());
  REQUIRE(mon->pit_lane_delta_s == Approx(21.5));
}

TEST_CASE("track_catalog_from_csv_stream handles spaces, comments and bad rows") {
  std::istringstream ss(csv_with_noise);
  auto cat = track_catalog_from_csv_stream(ss);
  REQUIRE(cat.size() == 2);
  REQUIRE(track_by_key_in(cat, "Bahrain").has_value());
  REQUIRE(track_by_key_in(cat, "Monaco").has_value());
}

TEST_CASE("load_track_catalog_csv returns nullopt on missing file") {
  auto none = load_track_catalog_csv("this_file_does_not_exist.csv");
  REQUIRE_FALSE(none.has_value());
}
