#pragma once
#include <cstdint>

namespace f1tm {

// Single immutable sample of world state for the client
struct SimSnapshot {
  double x = 0.0;             // world X (m)
  double y = 0.0;             // world Y (m)
  double heading_rad = 0.0;   // world heading (rad)
  double sim_time = 0.0;      // accumulated sim time (s)
  double s = 0.0;             // arc position along track (m)
  std::uint64_t lap = 0;      // completed laps
  std::uint64_t tick = 0;     // sim tick index
};

} // namespace f1tm
