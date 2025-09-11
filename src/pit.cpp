#include <f1tm/pit.hpp>
#include <algorithm>
#include <random>

namespace f1tm {

double pit_stop_loss(const PitParams& p) {
  const double stat = std::max(0.0, p.stationary);
  const double lane = std::max(0.0, p.lane);
  return stat + lane;
}

double pit_stop_loss_var(const PitParams& p, double epsilon, std::mt19937& rng) {
  const double base = pit_stop_loss(p);
  if (epsilon <= 0.0) return base;
  std::uniform_real_distribution<double> dist(-epsilon, +epsilon);
  return base + dist(rng);
}

static inline double clamp01(double x) {
  return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
}

// Generic: apply lane reduction factor in [0,1]
double pit_stop_loss_under(const PitParams& p, double lane_factor) {
  const double stat = std::max(0.0, p.stationary);
  const double lane = std::max(0.0, p.lane);
  const double f = clamp01(lane_factor);
  return stat + lane * f;
}

double pit_stop_loss_sc(const PitParams& p, double lane_factor) {
  return pit_stop_loss_under(p, lane_factor);
}

double pit_stop_loss_vsc(const PitParams& p, double lane_factor) {
  return pit_stop_loss_under(p, lane_factor);
}

} // namespace f1tm
