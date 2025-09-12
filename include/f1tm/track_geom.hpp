#pragma once
#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <numbers>

namespace f1tm {

// Constant naming convention (kCamelCase)
inline constexpr double kPI  = std::numbers::pi_v<double>;
inline constexpr double kTAU = 2.0 * kPI;

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

    // Find segment via binary search
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

    auto arc = [&](double cx, double cy, double a0, double a1, int steps){
      for (int i = 0; i <= steps; ++i) {
        double a = a0 + (a1 - a0) * (double(i)/double(steps));
        pts.push_back({ cx + R*std::cos(a), cy + R*std::sin(a) });
      }
    };

    // Start at (L, -R) bottom of right arc, go up around right arc to (L, +R)
    arc( L, 0.0, -kPI/2.0, +kPI/2.0, arc_pts_per_quadrant*2 );
    // Top straight to left arc start
    pts.push_back({ -L, +R });
    // Left arc (center -L,0): from +R down to -R
    arc( -L, 0.0, +kPI/2.0, 3.0*kPI/2.0, arc_pts_per_quadrant*2 );
    // Bottom straight back to right arc start
    pts.push_back({ +L, -R });

    // Close will repeat first
    return TrackPath{std::move(pts)};
  }

  // Build a smooth, closed track from control points using a uniform Catmull–Rom spline.
  // - ctrl: control polygon (closed; if not closed, we treat it as closed by wrapping)
  // - samples_per_seg: how many segments to generate between each pair of control points
  static TrackPath FromClosedCatmullRom(const std::vector<Vec2>& ctrl, int samples_per_seg = 24) {
    std::vector<Vec2> pts;
    const std::size_t n = ctrl.size();
    if (n < 3) return TrackPath{}; // need at least 3 points to get a curve

    // Treat as closed: wrap indices
    auto at = [&](std::ptrdiff_t i)->const Vec2& {
      std::ptrdiff_t k = (i % (std::ptrdiff_t)n + (std::ptrdiff_t)n) % (std::ptrdiff_t)n;
      return ctrl[(std::size_t)k];
    };

    // For each segment P1->P2, sample [0,1)
    for (std::size_t i = 0; i < n; ++i) {
      const Vec2& P0 = at((std::ptrdiff_t)i - 1);
      const Vec2& P1 = at((std::ptrdiff_t)i + 0);
      const Vec2& P2 = at((std::ptrdiff_t)i + 1);
      const Vec2& P3 = at((std::ptrdiff_t)i + 2);

      for (int s = 0; s < samples_per_seg; ++s) {
        double u = double(s) / double(samples_per_seg); // [0,1)
        pts.push_back(catmullRom_(P0, P1, P2, P3, u));
      }
    }
    // let set_points() close the loop by repeating the first point at the end
    return TrackPath{std::move(pts)};
  }

private:
  // Uniform Catmull–Rom (C1 continuous), stable and simple.
  static Vec2 catmullRom_(const Vec2& P0, const Vec2& P1, const Vec2& P2, const Vec2& P3, double u) {
    const double u2 = u*u;
    const double u3 = u2*u;
    // Basis matrix (0.5 * [ -1  3 -3  1;  2 -5  4 -1; -1  0  1  0;  0  2  0  0 ]) applied to [P0 P1 P2 P3]
    const double a0x = -P0.x + 3.0*P1.x - 3.0*P2.x + P3.x;
    const double a0y = -P0.y + 3.0*P1.y - 3.0*P2.y + P3.y;
    const double a1x =  2.0*P0.x - 5.0*P1.x + 4.0*P2.x - P3.x;
    const double a1y =  2.0*P0.y - 5.0*P1.y + 4.0*P2.y - P3.y;
    const double a2x = -P0.x + P2.x;
    const double a2y = -P0.y + P2.y;
    const double a3x =  2.0*P1.x;
    const double a3y =  2.0*P1.y;

    return Vec2{
      0.5*(a0x*u3 + a1x*u2 + a2x*u + a3x),
      0.5*(a0y*u3 + a1y*u2 + a2y*u + a3y)
    };
  }

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
