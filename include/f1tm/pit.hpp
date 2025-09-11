#pragma once
#include <random>

namespace f1tm {

struct PitParams {
  double stationary = 0.0; // seconds at box
  double lane = 0.0;       // pit-lane delta vs racing line
};

double pit_stop_loss(const PitParams& p);

// Stochastic variant around the base (uniform noise in [-epsilon, +epsilon])
double pit_stop_loss_var(const PitParams& p, double epsilon, std::mt19937& rng);

// Safety Car / Virtual Safety Car adjustments.
// Factor applies ONLY to lane component; stationary is unchanged.
// factor is clamped to [0, 1].
double pit_stop_loss_under(const PitParams& p, double lane_factor);
double pit_stop_loss_sc(const PitParams& p, double lane_factor /* e.g., 0.45 */);
double pit_stop_loss_vsc(const PitParams& p, double lane_factor /* e.g., 0.75 */);

} // namespace f1tm
