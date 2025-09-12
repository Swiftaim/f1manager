#pragma once
#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>

namespace f1tm {

struct Vec2 {
  double x{};
  double y{};
};

// Closed polyline track path with arc-length parameterization.
class TrackPath {
public:
  TrackPath() = default;
  explicit TrackPath(std::vector<Vec2> pts) { set_points(std::move(pts)); }

  void set_points(std::vector<Vec2> pts) {
    pts_ = std::move(pts);
    if (pts_.size() < 2) { pts_.clear(); cum_.clear(); length_ = 0.0; return; }
    // Ensure closed (repeat first at end if not equal)
    if (pts_.front().x != pts_.back().x || pts_.front().y != pts_.back().y) {
      pts_.push_back(pts_.front());
    }
    build_cumulative_();
  }

  const std::vector<Vec2>& points() const { return pts_; }
  double length() const { return length_; }
  bool empty() const { return pts_.size() < 2; }

  // Sample s in [0, length) to world position and heading (tangent angle).
  void sample_pose(double s, double& x, double& y, double& heading_rad) const {
    if (empty() || length_ <= 0.0) { x = y = heading_rad = 0.0; return; }
    // wrap s
    double sw = std::fmod(s, length_);
    if (sw < 0.0) sw += length_;

    // Find segment via linear scan or binary search
    // cum_[i] is the cumulative length *at point i* (0..N-1), with cum_[N-1] = length_
    auto it = std::upper_bound(cum_.begin(), cum_.end(), sw);
    std::size_t i1 = std::clamp<std::size_t>(std::distance(cum_.begin(), it), 1, pts_.size()-1);
    std::size_t i0 = i1 - 1;

    const double s0 = cum_[i0];
    const double seg_len = cum_[i1] - cum_[i0];
    const double t = (seg_len > 0.0) ? (sw - s0) / seg_len : 0.0;

    const Vec2& a = pts_[i0];
    const Vec2& b = pts_[i1];

    x = a.x + (b.x - a.x) * t;
    y = a.y + (b.y - a.y) * t;

    const double dx = (b.x - a.x);
    const double dy = (b.y - a.y);
    heading_rad = std::atan2(dy, dx);
  }

  // Factory: rounded-rectangle "stadium" track centered at (0,0)
  // straight_len: length of each straight section (centerline)
  // radius: corner radius (centerline)
  static TrackPath Stadium(double straight_len, double radius, int arc_pts_per_quadrant = 12) {
    std::vector<Vec2> pts;
    const double R = radius;
    const double L = straight_len * 0.5;
    // Build starting at rightmost point and go CCW.
    // Top arc center (L, 0), bottom arc center (-L, 0).
    auto arc = [&](double cx, double cy, double a0, double a1, int steps){
      for (int i = 0; i <= steps; ++i) {
        double a = a0 + (a1 - a0) * (double(i)/double(steps));
        pts.push_back({ cx + R*std::cos(a), cy + R*std::sin(a) });
      }
    };
    // Start at (L, -R) bottom of right arc, go up around right arc to (L, +R)
    arc( L, 0.0, -M_PI/2.0, +M_PI/2.0, arc_pts_per_quadrant*2 );
    // Top straight to left arc start
    pts.push_back({ -L, +R });
    // Left arc (center -L,0): from +R down to -R
    arc( -L, 0.0, +M_PI/2.0, +M_PI*1.5, arc_pts_per_quadrant*2 );
    // Bottom straight back to right arc start
    pts.push_back({ +L, -R });
    // Close will repeat first
    return TrackPath{std::move(pts)};
  }

private:
  void build_cumulative_() {
    cum_.resize(pts_.size());
    cum_[0] = 0.0;
    for (std::size_t i = 1; i < pts_.size(); ++i) {
      const double dx = pts_[i].x - pts_[i-1].x;
      const double dy = pts_[i].y - pts_[i-1].y;
      cum_[i] = cum_[i-1] + std::sqrt(dx*dx + dy*dy);
    }
    length_ = cum_.back();
  }

  std::vector<Vec2> pts_;
  std::vector<double> cum_;
  double length_{0.0};
};

} // namespace f1tm
