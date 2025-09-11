#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <optional>
#include <string>

#include <f1tm/track.hpp>
#include <f1tm/pit.hpp>

using Catch::Approx;
using namespace f1tm;

TEST_CASE("Track: lookup and pit helpers") {
  // Built-in tiny catalog should include Bahrain and Monaco as examples.
  auto t_bah = track_by_key("Bahrain");
  REQUIRE(t_bah.has_value());
  REQUIRE(t_bah->key == "Bahrain");

  // From track we can derive pit params and SC/VSC adjusted losses.
  const PitParams pit = track_pit_params(*t_bah);
  REQUIRE(pit.stationary >= 2.0);  // sanity, not zero
  REQUIRE(pit.lane > 10.0);        // Bahrain is a mid/high lane delta

  // SC/VSC factors should reduce lane component only.
  const double base = pit_stop_loss(pit);
  const double sc   = pit_stop_loss_under(pit, t_bah->sc_lane_factor);
  const double vsc  = pit_stop_loss_under(pit, t_bah->vsc_lane_factor);

  REQUIRE(sc  <= base);
  REQUIRE(vsc <= base);
  REQUIRE(sc  <= vsc); // SC typically cheaper than VSC
}

TEST_CASE("Track: unknown key returns nullopt") {
  auto t = track_by_key("NowhereGP");
  REQUIRE_FALSE(t.has_value());
}
