#include <f1tm/race.hpp>
#include <algorithm>

namespace f1tm {

static inline bool sizes_ok_stints_pits(const std::vector<StintParams>& stints,
                                        const std::vector<PitParams>& pits) {
  if (stints.empty()) return false;
  return pits.size() + 1 == stints.size();
}

std::optional<double> race_time(const std::vector<StintParams>& stints) {
  if (stints.empty()) return std::nullopt;
  double sum = 0.0;
  for (const auto& s : stints) {
    sum += estimate_stint_time(s);
  }
  return sum;
}

std::optional<double> race_time_with_pits(const std::vector<StintParams>& stints,
                                          const std::vector<PitParams>& pits) {
  if (!sizes_ok_stints_pits(stints, pits)) return std::nullopt;
  double sum = 0.0;
  for (size_t i = 0; i < stints.size(); ++i) {
    sum += estimate_stint_time(stints[i]);
    if (i + 1 < stints.size()) {
      sum += pit_stop_loss(pits[i]);
    }
  }
  return sum;
}

static inline double clamp01(double x) {
  return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
}

std::optional<double> race_time_with_pits_under(const std::vector<StintParams>& stints,
                                                const std::vector<PitParams>& pits,
                                                const std::vector<double>& lane_factors) {
  if (!sizes_ok_stints_pits(stints, pits)) return std::nullopt;
  if (lane_factors.size() != pits.size()) return std::nullopt;

  double sum = 0.0;
  for (size_t i = 0; i < stints.size(); ++i) {
    sum += estimate_stint_time(stints[i]);
    if (i + 1 < stints.size()) {
      const double f = clamp01(lane_factors[i]);
      // factor applies to lane only
      const double stat = std::max(0.0, pits[i].stationary);
      const double lane = std::max(0.0, pits[i].lane);
      sum += stat + lane * f;
    }
  }
  return sum;
}

static inline std::string upper(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

std::vector<double> lane_factors_from_events(const Track& track,
                                             const std::vector<std::string>& events) {
  std::vector<double> out;
  out.reserve(events.size());
  for (auto e : events) {
    const auto E = upper(e);
    if (E == "SC")       out.push_back(track.sc_lane_factor);
    else if (E == "VSC") out.push_back(track.vsc_lane_factor);
    else if (E == "GREEN" || E.empty()) out.push_back(1.0);
    else out.push_back(std::numeric_limits<double>::quiet_NaN()); // sentinel for unknown
  }
  return out;
}

std::optional<double> race_time_with_track(const std::vector<StintParams>& stints,
                                           const Track& track,
                                           const std::vector<std::string>& events) {
  if (stints.empty()) return std::nullopt;
  if (events.size() + 1 != stints.size()) return std::nullopt;

  // Map events to lane factors
  const auto lane_factors = lane_factors_from_events(track, events);
  // Validate: no unknowns
  for (double f : lane_factors) {
    if (std::isnan(f)) return std::nullopt;
  }

  // Build a pits vector with identical PitParams per stop, then apply factors
  std::vector<PitParams> pits(events.size(), track_pit_params(track));
  return race_time_with_pits_under(stints, pits, lane_factors);
}

} // namespace f1tm
