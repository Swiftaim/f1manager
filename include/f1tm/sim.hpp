#pragma once
#include <cstdint>
#include <cmath>

namespace f1tm {

// Parametric circular track (meters). First slice keeps it simple.
struct TrackCircle {
  double center_x = 0.0;
  double center_y = 0.0;
  double radius_m = 50.0;    // 50 m nominal
  double circumference_m() const { return 2.0 * 3.14159265358979323846 * radius_m; }
};

struct CarState {
  double s = 0.0;            // arc position along track [0, circumference)
  double speed_mps = 50.0;   // constant speed, meters per second
  std::uint64_t laps = 0;    // completed lap count
};

struct SimServer {
  TrackCircle track;
  CarState car;

  // Fixed-step simulation
  void step(double dt_sec) {
    const double C = track.circumference_m();
    if (C <= 0.0 || dt_sec <= 0.0 || car.speed_mps <= 0.0) return;
    double ds = car.speed_mps * dt_sec;           // meters advanced
    car.s += ds;
    while (car.s >= C) {
      car.s -= C;
      ++car.laps;
    }
  }

  // Convert arclength s → world-space (x,y,heading_rad) on the circle.
  void sample_pose(double& x, double& y, double& heading_rad) const {
    const double C = track.circumference_m();
    if (C <= 0.0) { x = y = 0.0; heading_rad = 0.0; return; }
    const double t = (car.s / C) * 2.0 * 3.14159265358979323846; // angle
    x = track.center_x + track.radius_m * std::cos(t);
    y = track.center_y + track.radius_m * std::sin(t);
    heading_rad = t + 3.14159265358979323846 / 2.0; // tangent orientation
  }
};

} // namespace f1tm
