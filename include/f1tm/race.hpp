// include/f1tm/race.hpp
#pragma once
#include <optional>
#include <string>
#include <vector>
#include <f1tm/stint.hpp>
#include <f1tm/pit.hpp>
#include <f1tm/track.hpp>

namespace f1tm {

// Existing APIs...
std::optional<double> race_time(const std::vector<StintParams>& stints);
std::optional<double> race_time_with_pits(const std::vector<StintParams>& stints,
                                          const std::vector<PitParams>& pits);
std::optional<double> race_time_with_pits_under(const std::vector<StintParams>& stints,
                                                const std::vector<PitParams>& pits,
                                                const std::vector<double>& lane_factors);

// NEW: Map event strings to lane factors using Track.
// Supported events (case-insensitive): "SC", "VSC", "GREEN" (or "")
// Returns a vector of factors same length as events.
std::vector<double> lane_factors_from_events(const Track& track,
                                             const std::vector<std::string>& events);

// NEW: Convenience — compose stints + pits derived from Track + events.
// Returns nullopt if sizes mismatch or an event is unknown.
std::optional<double> race_time_with_track(const std::vector<StintParams>& stints,
                                           const Track& track,
                                           const std::vector<std::string>& events);

} // namespace f1tm
