#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <numbers>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <f1tm/snap.hpp>

namespace f1tm {

class InterpBuffer {
public:
  static constexpr std::size_t kMaxCap = 128;

  explicit InterpBuffer(std::size_t cap = 64)
    : cap_(cap <= kMaxCap ? cap : kMaxCap) {}

  void push(const SimSnapshot& s) {
    buf_[write_index()] = s;
    ++size_;
  }

  // Interpolate entire snapshot at target_time.
  // - Outputs cars present in A or B (if one side missing -> clamp to that side).
  // - Telemetry (last/best and gap/sector) uses "newer side" dominance (and min for best).
  bool sample(double target_time, SimSnapshot& out) const {
    const std::size_t n = current_size();
    if (n == 0) return false;
    if (n == 1) { out = buf_[index(0)]; return true; }

    const auto& first = buf_[index(0)];
    const auto& last  = buf_[index(n-1)];
    if (target_time <= first.sim_time) { out = first; return true; }
    if (target_time >= last.sim_time)  { out = last;  return true; }

    // Find bracket [A,B]
    std::size_t lo = 0, hi = 1;
    for (; hi < n; ++hi) {
      const auto& a = buf_[index(hi-1)];
      const auto& b = buf_[index(hi)];
      if (target_time >= a.sim_time && target_time <= b.sim_time) {
        lo = hi - 1;
        break;
      }
    }

    const auto& A = buf_[index(lo)];
    const auto& B = buf_[index(hi)];
    const double dt = (B.sim_time - A.sim_time);
    const double t  = dt > 0.0 ? (target_time - A.sim_time) / dt : 0.0;

    // Interpolate snapshot metadata
    out.sim_time = A.sim_time + (B.sim_time - A.sim_time) * t;
    out.tick     = (t < 1.0 ? A.tick : B.tick);

    // Primary (back-compat) fields interpolate even if no car poses are present
    out.x = lerp(A.x, B.x, t);
    out.y = lerp(A.y, B.y, t);
    out.s = lerp(A.s, B.s, t);
    out.heading_rad = lerp_angle_shortest(A.heading_rad, B.heading_rad, t);
    out.lap = (t < 1.0 ? A.lap : B.lap);

    // Build car id set
    std::unordered_map<CarId, CarPose> a_map;
    std::unordered_map<CarId, CarPose> b_map;
    a_map.reserve(A.cars.size());
    b_map.reserve(B.cars.size());
    for (const auto& c : A.cars) a_map[c.id] = c;
    for (const auto& c : B.cars) b_map[c.id] = c;

    // Union ids
    std::vector<CarId> ids; ids.reserve(a_map.size() + b_map.size());
    for (const auto& kv : a_map) ids.push_back(kv.first);
    for (const auto& kv : b_map) if (!a_map.count(kv.first)) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    out.cars.clear();
    out.cars.reserve(ids.size());

    for (CarId id : ids) {
      const bool in_a = (a_map.find(id) != a_map.end());
      const bool in_b = (b_map.find(id) != b_map.end());
      CarPose cp{};
      if (in_a && in_b) {
        const auto& ca = a_map[id];
        const auto& cb = b_map[id];
        cp.id = id;
        cp.x  = lerp(ca.x, cb.x, t);
        cp.y  = lerp(ca.y, cb.y, t);
        cp.s  = lerp(ca.s, cb.s, t);
        cp.heading_rad = lerp_angle_shortest(ca.heading_rad, cb.heading_rad, t);
        cp.lap = (t < 1.0 ? ca.lap : cb.lap);

        const bool newer_is_b = (t >= 0.5);
        const auto& newer = newer_is_b ? cb : ca;

        // Telemetry propagation
        cp.last_lap_time = newer.last_lap_time;
        cp.best_lap_time = combine_min_(ca.best_lap_time, cb.best_lap_time);

        cp.gap_to_leader_m = newer.gap_to_leader_m;
        cp.gap_to_leader_s = newer.gap_to_leader_s;

        cp.s1_last = newer.s1_last; cp.s2_last = newer.s2_last; cp.s3_last = newer.s3_last;
        cp.s1_best = combine_min_(ca.s1_best, cb.s1_best);
        cp.s2_best = combine_min_(ca.s2_best, cb.s2_best);
        cp.s3_best = combine_min_(ca.s3_best, cb.s3_best);
      } else if (in_a) {
        cp = a_map[id];
      } else {
        cp = b_map[id];
      }
      out.cars.push_back(cp);
    }

    // Back-compat fill primary fields from car id 0 if present, else first
    if (!out.cars.empty()) {
      const CarPose* primary = nullptr;
      for (const auto& c : out.cars) if (c.id == 0) { primary = &c; break; }
      if (!primary) primary = &out.cars.front();
      out.x = primary->x;
      out.y = primary->y;
      out.s = primary->s;
      out.lap = primary->lap;
      out.heading_rad = primary->heading_rad;
    }

    return true;
  }

  double latest_time() const {
    const std::size_t n = current_size();
    return n == 0 ? 0.0 : buf_[index(n-1)].sim_time;
  }

private:
  static double lerp(double a, double b, double t) { return a + (b - a) * t; }
  static double combine_min_(double a, double b) {
    if (a < 0.0) return b;
    if (b < 0.0) return a;
    return (a < b) ? a : b;
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
    double d = b - a;
    if (d >  kPI) d -= kTAU;
    if (d < -kPI) d += kTAU;
    return norm_angle(a + d * t);
  }

  std::size_t index(std::size_t logical) const {
    const std::size_t n = current_size();
    const std::size_t start = (size_ >= cap_)
      ? static_cast<std::size_t>(size_ % static_cast<std::uint64_t>(cap_))
      : 0u;
    return (start + logical) % cap_;
  }
  std::size_t write_index() const {
    return static_cast<std::size_t>(size_ % static_cast<std::uint64_t>(cap_));
  }
  std::size_t current_size() const {
    const std::uint64_t cap64 = static_cast<std::uint64_t>(cap_);
    const std::uint64_t used  = (size_ < cap64) ? size_ : cap64;
    return static_cast<std::size_t>(used);
  }

  // Capacity (<= kMaxCap)
  std::size_t cap_;
  // Ring buffer
  std::array<SimSnapshot, kMaxCap> buf_{};
  std::uint64_t size_{0};
};

} // namespace f1tm
