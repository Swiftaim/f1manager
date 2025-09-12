#pragma once
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>
#include <string>
#include <f1tm/sim.hpp>
#include <f1tm/snap.hpp>
#include <f1tm/snap_buffer.hpp>
#include <f1tm/track_geom.hpp>

namespace f1tm {

enum class TrackPreset : int {
  Stadium = 0,
  ChicaneHairpin = 1,
  Count
};

// Owns the simulation thread and publishes snapshots.
class SimRunner {
public:
  SimRunner() = default;
  ~SimRunner() { stop(); }
  SimRunner(const SimRunner&) = delete;
  SimRunner& operator=(const SimRunner&) = delete;

  void start();
  void stop();

  // Basic world setup and car population
  void configure_default_world();      // sets stadium track and 8 cars by default
  void set_default_cars(std::size_t n);// redefines initial cars (call before start)
  void request_reseed(std::size_t n);  // hot-reset while running (resets sim_time/tick)

  // Track presets
  void request_track_preset(TrackPreset p); // safe to call from UI thread
  const TrackPath& track_path() const { return path_; }
  TrackPreset current_preset() const { return preset_; }
  const char* preset_name() const;

  SnapshotBuffer& buffer() { return buffer_; }
  const SnapshotBuffer& buffer() const { return buffer_; }

  // Control surface
  std::atomic<double> time_scale{1.0}; // 0.0 = paused

private:
  void thread_main_();
  static std::vector<double> stagger_s_(std::size_t n, double circumference);
  static TrackPath make_preset_(TrackPreset p);

  std::thread th_;
  std::atomic<bool> running_{false};

  // Sim & data sharing
  SnapshotBuffer buffer_;

  // World setup used by the thread
  TrackCircle track_{ .center_x = 0.0, .center_y = 0.0, .radius_m = 120.0 };
  TrackPath   path_{}; // stadium / arbitrary
  TrackPreset preset_{TrackPreset::Stadium};
  struct CarInit { CarId id; double speed_mps; double s0; std::uint64_t laps0; };
  std::vector<CarInit> initial_cars_{{ {0, 70.0, 0.0, 0} }};

  // Hot reseed / preset change control
  std::atomic<bool> pending_reset_{false};
  std::atomic<std::size_t> pending_reset_n_{0};
  std::atomic<bool> pending_preset_change_{false};
  std::atomic<int>  pending_preset_{-1};
};

} // namespace f1tm
