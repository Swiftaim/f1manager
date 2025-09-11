#pragma once
#include <random>
#include <string>
#include <vector>
#include <f1tm/track.hpp>

namespace f1tm {

// Emit one event per pit window: "SC", "VSC", or "GREEN".
// Probabilities: p_sc >= 0, p_vsc >= 0. We clamp and ensure total <= 1.
// Deterministic with caller-provided rng.
std::vector<std::string> simulate_pit_events(std::size_t count,
                                             double p_sc,
                                             double p_vsc,
                                             std::mt19937& rng);

// Convenience: directly get lane factors using Track (SC/VSC/Green -> factors).
std::vector<double> simulate_lane_factors(std::size_t count,
                                          const Track& track,
                                          double p_sc,
                                          double p_vsc,
                                          std::mt19937& rng);

} // namespace f1tm
