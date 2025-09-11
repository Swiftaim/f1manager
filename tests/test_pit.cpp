#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <f1tm/pit.hpp>

using Catch::Approx;
using namespace f1tm;

TEST_CASE("pit_stop_loss") {
  SECTION("adds stationary and lane time") {
    PitParams p;
    p.stationary = 2.5;   // tyre change time
    p.lane = 17.0;        // pit lane delta vs staying on track
    REQUIRE(pit_stop_loss(p) == Approx(19.5));
  }

  SECTION("zero values allowed") {
    PitParams p{0.0, 0.0};
    REQUIRE(pit_stop_loss(p) == Approx(0.0));
  }

  SECTION("negative inputs clamp to zero") {
    PitParams p{-1.0, -10.0};
    REQUIRE(pit_stop_loss(p) == Approx(0.0));
  }
}

TEST_CASE("pit_stop_loss stochastic variance") {
  PitParams p{2.5, 17.0};   // base = 19.5
  const double eps = 0.5;

  SECTION("value stays within bounds") {
    std::mt19937 rng(12345);
    const double v = pit_stop_loss_var(p, eps, rng);
    REQUIRE(v >= 19.5 - eps);
    REQUIRE(v <= 19.5 + eps);
  }

  SECTION("deterministic with same seed") {
    std::mt19937 rng1(42);
    std::mt19937 rng2(42);
    const double v1 = pit_stop_loss_var(p, eps, rng1);
    const double v2 = pit_stop_loss_var(p, eps, rng2);
    REQUIRE(v1 == Catch::Approx(v2));
  }

  SECTION("different seeds likely produce different values") {
    std::mt19937 rng1(42);
    std::mt19937 rng2(43);
    const double v1 = pit_stop_loss_var(p, eps, rng1);
    const double v2 = pit_stop_loss_var(p, eps, rng2);
    REQUIRE(v1 != Catch::Approx(v2)); // statistical, but very likely with uniform
  }

  SECTION("zero epsilon returns base loss") {
    std::mt19937 rng(999);
    REQUIRE(pit_stop_loss_var(p, 0.0, rng) == Catch::Approx(pit_stop_loss(p)));
  }

  SECTION("negative epsilon treated as zero") {
    std::mt19937 rng(999);
    REQUIRE(pit_stop_loss_var(p, -0.1, rng) == Catch::Approx(pit_stop_loss(p)));
  }
}

TEST_CASE("pit_stop_loss under SC and VSC") {
  const PitParams base{2.5, 17.0}; // stationary=2.5, lane=17.0, normal total=19.5

  SECTION("SC reduces lane delta via factor") {
    // Example: 45% of normal lane loss under full Safety Car
    const double sc = pit_stop_loss_sc(base, 0.45);
    // Expected: stationary + lane*0.45 = 2.5 + 17.0*0.45 = 10.15
    REQUIRE(sc == Approx(10.15));
  }

  SECTION("VSC reduces lane delta less than SC") {
    // Example: 75% of normal lane loss under VSC
    const double vsc = pit_stop_loss_vsc(base, 0.75);
    // Expected: 2.5 + 17.0*0.75 = 15.25
    REQUIRE(vsc == Approx(15.25));
  }

  SECTION("factors are clamped to [0,1]") {
    REQUIRE(pit_stop_loss_sc(base, -0.3) == Approx(2.5));     // lane fully neutralized
    REQUIRE(pit_stop_loss_sc(base, 1.7)  == Approx(19.5));    // equals normal loss
  }

  SECTION("zero lane or zero stationary behaves sensibly") {
    REQUIRE(pit_stop_loss_sc(PitParams{0.0, 12.0}, 0.5) == Approx(6.0));
    REQUIRE(pit_stop_loss_sc(PitParams{3.0, 0.0}, 0.5)  == Approx(3.0));
  }
}
