#pragma once
#include <unordered_map>
#include <cstdint>
#include <f1tm/sim.hpp>

namespace f1tm {

struct TelemetryTimes {
  double last_lap{-1.0};
  double best_lap{-1.0};
  std::uint64_t laps{0};
  // Sector times
  double s_last[3]{-1.0,-1.0,-1.0};
  double s_best[3]{-1.0,-1.0,-1.0};
};

class TelemetrySink {
public:
  void init_if_needed(const class SimServer& sim, double now_time) {
    if (initialized_) return;
    initialized_ = true;
    const std::size_t n = sim.car_count();
    for (std::size_t i = 0; i < n; ++i) {
      if (const auto* c = sim.car_by_index(i)) {
        State st{};
        st.lap_start_time = now_time; // will be reset on first crossing
        st.sector_start_time = now_time;
        st.laps = c->laps;
        st.started = false;           // ignore first increment; start timing from there
        st.last_s = c->s;
        st.next_sector_idx = 0;
        cars_[c->id] = st;
      }
    }
  }

  void update(const class SimServer& sim, double now_time) {
    init_if_needed(sim, now_time);
    const double C = sim.track_length();
    const double S1 = C / 3.0;
    const double S2 = 2.0 * C / 3.0;

    const std::size_t n = sim.car_count();
    for (std::size_t i = 0; i < n; ++i) {
      const auto* c = sim.car_by_index(i);
      if (!c) continue;
      auto& st = cars_[c->id]; // default constructed if not present

      const double prev_prog = st.laps * C + st.last_s;
      const double now_prog  = c->laps * C + c->s;

      // Process sector boundaries S1 and S2 (S3 handled at lap increment)
      if (st.started) {
        // Evaluate whether we've crossed S1 or S2; possibly more than one if dt large
        while (st.next_sector_idx < 2) {
          const double boundary_m = st.laps * C + ((st.next_sector_idx == 0) ? S1 : S2);
          if (now_prog >= boundary_m - 1e-9) {
            const double sector_time = now_time - st.sector_start_time;
            st.s_last[st.next_sector_idx] = sector_time;
            if (st.s_best[st.next_sector_idx] < 0.0 || sector_time < st.s_best[st.next_sector_idx]) {
              st.s_best[st.next_sector_idx] = sector_time;
            }
            st.sector_start_time = now_time;
            ++st.next_sector_idx;
          } else {
            break;
          }
        }
      }

      // Detect lap completion via lap counter increment
      if (c->laps > st.laps) {
        if (!st.started) {
          // First time crossing start/finish; start timing from *now* and do not emit a lap or S3.
          st.started = true;
          st.lap_start_time = now_time;
          st.sector_start_time = now_time;
          st.next_sector_idx = 0;
        } else {
          // Complete S3
          const double s3_time = now_time - st.sector_start_time;
          st.s_last[2] = s3_time;
          if (st.s_best[2] < 0.0 || s3_time < st.s_best[2]) {
            st.s_best[2] = s3_time;
          }
          // Complete lap
          const double lap_time = now_time - st.lap_start_time;
          st.last_lap_time = lap_time;
          if (st.best_lap_time < 0.0 || lap_time < st.best_lap_time) {
            st.best_lap_time = lap_time;
          }
          // Reset for next lap
          st.lap_start_time = now_time;
          st.sector_start_time = now_time;
          st.next_sector_idx = 0;
        }
        st.laps = c->laps;
      }

      st.last_s = c->s;
    }
  }

  bool get(CarId id, TelemetryTimes& out) const {
    auto it = cars_.find(id);
    if (it == cars_.end()) return false;
    out.last_lap = it->second.last_lap_time;
    out.best_lap = it->second.best_lap_time;
    out.laps     = it->second.laps;
    for (int k=0;k<3;++k) { out.s_last[k] = it->second.s_last[k]; out.s_best[k] = it->second.s_best[k]; }
    return true;
  }

private:
  struct State {
    double lap_start_time{0.0};
    double sector_start_time{0.0};
    double last_lap_time{-1.0};
    double best_lap_time{-1.0};
    std::uint64_t laps{0};
    double last_s{0.0};
    bool started{false};
    int  next_sector_idx{0}; // 0->S1, 1->S2, (S3 handled at lap)
    double s_last[3]{-1.0,-1.0,-1.0};
    double s_best[3]{-1.0,-1.0,-1.0};
  };
  std::unordered_map<CarId, State> cars_;
  bool initialized_{false};
};

} // namespace f1tm
