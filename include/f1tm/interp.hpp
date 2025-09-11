#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <cmath>
#include <numbers>
#include <f1tm/snap.hpp>

namespace f1tm {

// Keep last N snapshots; sample by sim_time with clamping.
// No locking here — use on the client thread after pulling from SnapshotBuffer.
class InterpBuffer {
public:
  explicit InterpBuffer(std::size_t cap = 64) : cap_(cap) {}

  void push(const SimSnapshot& s) {
    if (size_ < cap_) {
      buf_[size_ % cap_] = s;
      ++size_;
    } else {
      buf_[write_index()] = s;
      ++size_;
    }
  }

  // Sample snapshot at target time (seconds); returns false if empty.
  // Policy: clamp outside range to nearest endpoint; no extrapolation.
  bool sample(double target_time, SimSnapshot& out) const {
    if (size_ == 0) return false;
    if (size_ == 1) { out = buf_[index(0)]; return true; }

    // Find bracketing snapshots by sim_time (buffer is append-only in time)
    // Convert logical indices to physical ring positions
    std::size_t n = static_cast<std::size_t>(std::min<size_t>(size_, cap_));
    // earliest .. latest
    const SimSnapshot& first = buf_[index(0)];
    const SimSnapshot& last  = buf_[index(n-1)];
    if (target_time <= first.sim_time) { out = first; return true; }
    if (target_time >= last.sim_time)  { out = last;  return true; }

    // Linear search is fine for tiny N; can upgrade to binary if needed
    std::size_t lo = 0, hi = 1;
    for (; hi < n; ++hi) {
      const auto& a = buf_[index(hi-1)];
      const auto& b = buf_[index(hi)];
      if (target_time >= a.sim_time && target_time <= b.sim_time) {
        lo = hi-1;
        break;
      }
    }
    const auto& A = buf_[index(lo)];
    const auto& B = buf_[index(hi)];
    const double dt = (B.sim_time - A.sim_time);
    const double t  = dt > 0.0 ? (target_time - A.sim_time) / dt : 0.0;

    out = lerp_snapshot(A, B, t);
    return true;
  }

  // Convenience: latest known time
  double latest_time() const {
    if (size_ == 0) return 0.0;
    std::size_t n = static_cast<std::size_t>(std::min<size_t>(size_, cap_));
    return buf_[index(n-1)].sim_time;
  }

private:
  // Linear interpolation helper (shortest-angle for heading)
  static SimSnapshot lerp_snapshot(const SimSnapshot& a, const SimSnapshot& b, double t) {
    auto lerp = [](double u, double v, double t){ return u + (v - u) * t; };

    SimSnapshot o{};
    o.sim_time = lerp(a.sim_time, b.sim_time, t);
    o.x        = lerp(a.x,        b.x,        t);
    o.y        = lerp(a.y,        b.y,        t);
    o.s        = lerp(a.s,        b.s,        t);
    o.heading_rad = lerp_angle_shortest(a.heading_rad, b.heading_rad, t);
    // Discrete fields: choose earlier (monotone floor)
    o.lap  = (t < 1.0 ? a.lap : b.lap);
    o.tick = (t < 1.0 ? a.tick: b.tick);
    return o;
  }

  static double norm_angle(double a) {
    const double kTAU = std::numbers::pi_v<double> * 2.0;
    a = std::fmod(a, kTAU);
    if (a < 0.0) a += kTAU;
    return a;
  }

  static double lerp_angle_shortest(double a, double b, double t) {
    const double kPI  = std::numbers::pi_v<double>;
    const double kTAU = std::numbers::pi_v<double> * 2.0;
    a = norm_angle(a);
    b = norm_angle(b);
    double delta = b - a;
    if (delta >  kPI)  delta -= kTAU;
    if (delta < -kPI)  delta += kTAU;
    double res = a + delta * t;
    return norm_angle(res);
  }

  // Ring helpers
  std::size_t index(std::size_t logical) const {
    // logical 0 is oldest of the current window
    std::size_t n = static_cast<std::size_t>(std::min<size_t>(size_, cap_));
    std::size_t start = (size_ >= cap_) ? (size_ % cap_) : 0;
    return (start + logical) % cap_;
  }
  std::size_t write_index() const { return size_ % cap_; }

  std::size_t cap_;
  // fixed upper bound storage (64 default): use a static array big enough
  std::array<SimSnapshot, 256> buf_{}; // capacity max; we only use first cap_ slots
  std::uint64_t size_{0};              // logical number of pushes since start
};

} // namespace f1tm
