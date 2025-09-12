#pragma once
#include <cstdint>
#include <vector>
#include <optional>
#include <limits>

#include <f1tm/sim.hpp> // for CarId

namespace f1tm {

struct CarPose {
  CarId id{};
  double x{};
  double y{};
  double heading_rad{};
  double s{};
  std::uint64_t lap{};

  // Telemetry (server-filled; -1.0 means "unknown/not set yet")
  double last_lap_time{-1.0};
  double best_lap_time{-1.0};

  // Gap to leader (meters & seconds). Leader has 0.0. Unknown -> -1.0
  double gap_to_leader_m{-1.0};
  double gap_to_leader_s{-1.0};

  // Sector times (last completed sector and best sector).
  double s1_last{-1.0}, s2_last{-1.0}, s3_last{-1.0};
  double s1_best{-1.0}, s2_best{-1.0}, s3_best{-1.0};
};

// Multi-car snapshot keyed by sim_time.
// Backward-compatible fields (x,y,heading_rad,s,lap) mirror car id=0 if present.
struct SimSnapshot {
  double sim_time{};
  std::uint64_t tick{};

  // Multi-car set
  std::vector<CarPose> cars{};

  // --- Back-compat (primary car) ---
  double x{};
  double y{};
  double heading_rad{};
  double s{};
  std::uint64_t lap{};
};

// Helper: find pose by id in a snapshot
inline std::optional<CarPose> find_car(const SimSnapshot& ss, CarId id) {
  for (const auto& c : ss.cars) if (c.id == id) return c;
  return std::nullopt;
}

} // namespace f1tm
