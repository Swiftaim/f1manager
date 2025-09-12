#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <numbers>
#include <cmath>
#include <optional>
#include <f1tm/track_geom.hpp>

namespace f1tm {

using CarId = std::uint32_t;

// Parametric circular track (meters).
struct TrackCircle {
  double center_x = 0.0;
  double center_y = 0.0;
  double radius_m = 50.0;    // 50 m nominal
  double circumference_m() const { return 2.0 * std::numbers::pi_v<double> * radius_m; }
};

struct CarState {
  CarId id = 0;
  double s = 0.0;            // arc position along track [0, circumference)
  double speed_mps = 50.0;   // meters per second
  std::uint64_t laps = 0;    // completed lap count
};

// Authoritative simulation server (supports circle or path).
class SimServer {
public:
  TrackCircle track;       // legacy circle
  void set_track_path(const TrackPath& p) { path_ = p; use_path_ = true; }
  void clear_track_path() { use_path_ = false; path_ = TrackPath{}; }
  const std::optional<TrackPath>& track_path() const { return path_; }

  // --- Car management
  void clear_cars() { cars_.clear(); }
  void add_car(CarId id, double speed_mps, double s0 = 0.0, std::uint64_t laps0 = 0);
  std::size_t car_count() const { return cars_.size(); }

  // Access by index (0..N-1). Returns nullptr if out of range.
  const CarState* car_by_index(std::size_t idx) const;
  CarState*       car_by_index(std::size_t idx);

  // Access by id (linear search; fine for small N)
  const CarState* car_by_id(CarId id) const;
  CarState*       car_by_id(CarId id);

  // --- Simulation
  void step(double dt_sec);

  // Sample arclength s -> world-space (x,y,heading_rad)
  void sample_pose(double& x, double& y, double& heading_rad) const;
  void sample_pose_index(std::size_t idx, double& x, double& y, double& heading_rad) const;
  bool sample_pose_for(CarId id, double& x, double& y, double& heading_rad) const;

  // Total length
  double track_length() const;

private:
  std::vector<CarState> cars_;
  std::optional<TrackPath> path_{};
  bool use_path_{false};

  static void s_to_pose_circle(const TrackCircle& trk, double s, double& x, double& y, double& heading_rad) {
    const double C = trk.circumference_m();
    if (C <= 0.0) { x = y = 0.0; heading_rad = 0.0; return; }
    const double t = (s / C) * (2.0 * std::numbers::pi_v<double>); // angle
    x = trk.center_x + trk.radius_m * std::cos(t);
    y = trk.center_y + trk.radius_m * std::sin(t);
    heading_rad = t + (std::numbers::pi_v<double> / 2.0); // tangent orientation
  }
};

} // namespace f1tm
