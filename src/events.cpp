#include <f1tm/events.hpp>
#include <algorithm>
#include <random>
#include <cctype>
#include <f1tm/race.hpp> // for lane_factors_from_events

namespace f1tm {

static inline double clamp01(double x) {
  return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
}

std::vector<std::string> simulate_pit_events(std::size_t count,
                                             double p_sc,
                                             double p_vsc,
                                             std::mt19937& rng) {
  // Clamp and cap total to 1.0
  double sc  = clamp01(p_sc);
  double vsc = clamp01(p_vsc);
  double total = sc + vsc;
  if (total > 1.0) {
    // renormalize proportionally to keep SC:VSC ratio
    sc  /= total;
    vsc /= total;
    total = 1.0;
  }

  std::uniform_real_distribution<double> U(0.0, 1.0);
  std::vector<std::string> out;
  out.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const double u = U(rng);
    if (u < sc) out.emplace_back("SC");
    else if (u < sc + vsc) out.emplace_back("VSC");
    else out.emplace_back("GREEN");
  }
  return out;
}

std::vector<double> simulate_lane_factors(std::size_t count,
                                          const Track& track,
                                          double p_sc,
                                          double p_vsc,
                                          std::mt19937& rng) {
  auto events = simulate_pit_events(count, p_sc, p_vsc, rng);
  return lane_factors_from_events(track, events);
}

} // namespace f1tm
