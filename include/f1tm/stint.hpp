#pragma once
#include <cstddef>

namespace f1tm {

struct StintParams {
  int laps = 0;           // number of laps in the stint
  double baseLap = 0.0;   // baseline lap time (seconds)
  double degradationPerLap = 0.0; // added seconds per lap
};

// Returns total stint time in seconds
double estimate_stint_time(const StintParams& p);

} // namespace f1tm
