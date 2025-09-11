#include <f1tm/stint.hpp>
#include <algorithm>

namespace f1tm {

double estimate_stint_time(const StintParams& p) {
  // Defensive clamps
  const int laps = std::max(0, p.laps);
  const double base = p.baseLap;
  const double deg = p.degradationPerLap;
  if (laps == 0 || base <= 0.0 || deg < 0.0) {
    // Simple guard: if base <= 0 or deg < 0 treat as invalid and return 0
    // (We can refine later; TDD will guide changes.)
    if (laps == 0) return 0.0;
    if (base <= 0.0) return 0.0;
    if (deg < 0.0) return 0.0;
  }

  // Arithmetic series: sum_{i=0}^{n-1} (base + i*deg)
  // = n*base + deg * n*(n-1)/2
  const double n = static_cast<double>(laps);
  return n * base + deg * n * (n - 1.0) * 0.5;
}

} // namespace f1tm
