#include <f1tm/sim_runner.hpp>
#include <chrono>
#include <algorithm>
#include <vector>
#include <f1tm/telemetry.hpp>

namespace f1tm {

static std::vector<double> grid_s_positions_(std::size_t n, double circumference,
                                             double row_gap_m, double lane_gap_m) {
  std::vector<double> s(n, 0.0);
  if (n == 0 || circumference <= 0.0) return s;
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t row  = i / 2;
    const std::size_t lane = i % 2; // 0 = pole side, 1 = off side
    const double back = row * row_gap_m + (lane == 1 ? lane_gap_m : 0.0);
    double pos = std::fmod((circumference - back), circumference);
    if (pos < 0.0) pos += circumference;
    s[i] = pos;
  }
  return s;
}

TrackPath SimRunner::make_preset_(TrackPreset p) {
  // Control polygon describing the general shape (closed loop, rough corners)
  std::vector<Vec2> ctrl;
  auto add = [&](double x, double y){ ctrl.push_back({x,y}); };
  
  // Increase samples_per_seg for even smoother corners.
  int samples_per_seg = 28;

  switch (p) {
    case TrackPreset::Stadium:
      return TrackPath::Stadium(/*straight_len*/ 250.0, /*radius*/ 80.0, /*arc detail*/ 14);
    case TrackPreset::ChicaneHairpin: {  
      // A compact GP-like shape: right vertical -> chicane -> long top -> hairpin -> bottom return
      add( 150, -60); add(150,  60);   // right side
      add(  40,  80); add(-10,  60);   // chicane in
      add( -40,  30); add(-120, 30);   // top straight
      add(-160,   0); add(-150, -60);  // hairpin approach
      add(-120, -100); add(-60, -110); // hairpin exit
      add(  40, -90); add(120, -80);   // back to start

      // Build a smooth closed path from control points.
      return TrackPath::FromClosedCatmullRom(ctrl, samples_per_seg);
    }

    case TrackPreset::GPVaried: {      
      // Bottom straight into braking
      add( 200, -100);
      add( 220,  -40);

      // Flowing Esses (right-left-right-left)
      add( 180,   20);
      add( 120,   60);
      add(  60,  100);
      add(   0,   60);
      add( -60,   20);
      add(-120,   50);

      // Carousel (sweeping, sustained corner on the left)
      add(-180,   40);
      add(-220,    0);
      add(-200,  -60);
      add(-140, -120);
      add( -60, -150);
      add(  40, -140);
      add( 120, -120);
      add( 180,  -110);
      add( 200,  -100);

      // Build smooth closed curve; raise samples for even rounder corners
      samples_per_seg = 30;
      return TrackPath::FromClosedCatmullRom(ctrl, samples_per_seg);
    }

    case TrackPreset::GPCustom: {
      // A compact GP-like shape: right vertical -> chicane -> long top -> hairpin -> bottom return
      add( 150, -60); add(220,  60);   // right side
      add( 160, 60); add(100,  60);   // top-right
      add(  60,  60); add(50,  20);   // down loop
      add(  100, 0); add(140,  0);   // chicane right
      add(  160,  -20); add(160,  -40);   // chicane down
      add(  120,  -60); add(80,  -60);   // chicane left
      add(  40,  -60); add(40,  -40);   // chicane left
      add(  20,  -40); add(0,  -20);   // chicane left
      add(  0,  0); add(20,  20);   // chicane left
      add(  60,  20); add(60,  60);   // chicane up
      add( -40,  50); add(-120, 40);   // top straight
      add(-160,   0); add(-150, -60);  // hairpin approach
      add(-120, -100); add(-60, -110); // hairpin exit
      add(  40, -90); add(120, -80);   // back to start

      // Build a smooth closed path from control points.
      return TrackPath::FromClosedCatmullRom(ctrl, samples_per_seg);
      // Build smooth closed curve; raise samples for even rounder corners
      samples_per_seg = 24;
      return TrackPath::FromClosedCatmullRom(ctrl, samples_per_seg);
    }


    default:
      return TrackPath::Stadium(250.0, 80.0, 14);
  }
}

const char* SimRunner::preset_name() const {
  switch (preset_) {
    case TrackPreset::Stadium:        return "Stadium";
    case TrackPreset::ChicaneHairpin: return "Chicane+Hairpin";
    case TrackPreset::GPVaried:       return "GP Varied (Esses+Carousel)"; // NEW
    default: return "Unknown";
  }
}

void SimRunner::configure_default_world() {
  preset_ = TrackPreset::Stadium;
  path_ = make_preset_(preset_);
  set_default_cars(8); // default to 8 cars
}

void SimRunner::set_default_cars(std::size_t n) {
  if (n == 0) n = 1;
  initial_cars_.clear();
  const double C = path_.empty() ? track_.circumference_m() : path_.length();
  // Typical F1 grid spacing along centerline ~9 m; small stagger between lanes ~3 m.
  const auto s_positions = grid_s_positions_(n, C, /*row_gap_m*/ 9.0, /*lane_gap_m*/ 3.0);
  for (std::size_t i = 0; i < n; ++i) {
    const double base = 62.0;                  // base speed
    const double jitter = 3.0 * double(i % 4); // 0,3,6,9 pattern
    initial_cars_.push_back(CarInit{
      static_cast<CarId>(i),
      base + jitter,
      s_positions[i],
      0
    });
  }
}

void SimRunner::request_reseed(std::size_t n) {
  pending_reset_n_.store(n, std::memory_order_relaxed);
  pending_reset_.store(true, std::memory_order_release);
}

void SimRunner::request_track_preset(TrackPreset p) {
  pending_preset_.store(static_cast<int>(p), std::memory_order_relaxed);
  pending_preset_change_.store(true, std::memory_order_release);
}

std::vector<double> SimRunner::stagger_s_(std::size_t n, double circumference) {
  std::vector<double> s(n, 0.0);
  if (n == 0 || circumference <= 0.0) return s;
  for (std::size_t i = 0; i < n; ++i) {
    s[i] = (circumference * double(i)) / double(n); // even arc spacing
  }
  return s;
}

void SimRunner::start() {
  if (running_.load()) return;
  running_.store(true);
  th_ = std::thread(&SimRunner::thread_main_, this);
}

void SimRunner::stop() {
  if (!running_.load()) return;
  running_.store(false);
  if (th_.joinable()) th_.join();
}

void SimRunner::thread_main_() {
  SimServer sim;
  sim.track = track_;
  if (!path_.empty()) sim.set_track_path(path_);
  sim.clear_cars();
  for (const auto& c : initial_cars_) {
    sim.add_car(c.id, c.speed_mps, c.s0, c.laps0);
  }

  TelemetrySink telem;

  using clock = std::chrono::steady_clock;
  const double base_dt = 1.0 / 240.0; // 240 Hz wall cadence
  const auto   tick_ns = std::chrono::nanoseconds((long long)(base_dt * 1e9));
  auto next = clock::now();
  double sim_time = 0.0;
  std::uint64_t tick = 0;

  while (running_.load(std::memory_order_relaxed)) {
    // Handle track preset change
    if (pending_preset_change_.load(std::memory_order_acquire)) {
      pending_preset_change_.store(false, std::memory_order_relaxed);
      const int ip = pending_preset_.load(std::memory_order_relaxed);
      if (ip >= 0) {
        preset_ = static_cast<TrackPreset>(ip);
        path_ = make_preset_(preset_);
        // Reset cars and timers
        set_default_cars(initial_cars_.size() ? initial_cars_.size() : 8);
        sim.clear_cars();
        sim.set_track_path(path_);
        for (const auto& c : initial_cars_) sim.add_car(c.id, c.speed_mps, c.s0, c.laps0);
        sim_time = 0.0;
        tick = 0;
        telem = TelemetrySink{}; // reset telemetry
      }
    }

    // Handle hot reseed request (car count)
    if (pending_reset_.load(std::memory_order_acquire)) {
      pending_reset_.store(false, std::memory_order_relaxed);
      const std::size_t n = pending_reset_n_.load(std::memory_order_relaxed);
      // Rebuild initial cars and reset sim (keep current preset/path)
      set_default_cars(n);
      sim.clear_cars();
      for (const auto& c : initial_cars_) sim.add_car(c.id, c.speed_mps, c.s0, c.laps0);
      sim_time = 0.0;
      tick = 0;
      telem = TelemetrySink{}; // reset telemetry
    }

    const double warp = time_scale.load(std::memory_order_relaxed);
    const double dt_eff = base_dt * (warp < 0.0 ? 0.0 : warp);

    if (dt_eff > 0.0) {
      sim.step(dt_eff);
      sim_time += dt_eff;
      ++tick;
    } else {
      ++tick; // publish heartbeats even when paused
    }

    // Update telemetry after ticking the sim
    telem.update(sim, sim_time);

    // Publish MULTI-CAR snapshot (with gaps & sectors)
    SimSnapshot s{};
    s.sim_time = sim_time;
    s.tick = tick;

    const std::size_t n = sim.car_count();
    s.cars.reserve(n);

    const double C = sim.track_length();
    std::vector<double> progress;
    progress.reserve(n);
    double leader_prog = -1.0;
    std::size_t leader_index = 0;

    // First pass: fill car poses and record progress
    for (std::size_t i = 0; i < n; ++i) {
      const auto* car = sim.car_by_index(i);
      if (!car) continue;
      double x, y, heading;
      sim.sample_pose_index(i, x, y, heading);
      CarPose cp{};
      cp.id = car->id;
      cp.x = x; cp.y = y; cp.heading_rad = heading;
      cp.s = car->s; cp.lap = car->laps;
      // Fill telemetry (laps + sectors)
      TelemetryTimes tt{};
      if (telem.get(cp.id, tt)) {
        cp.last_lap_time = tt.last_lap;
        cp.best_lap_time = tt.best_lap;
        cp.s1_last = tt.s_last[0]; cp.s2_last = tt.s_last[1]; cp.s3_last = tt.s_last[2];
        cp.s1_best = tt.s_best[0]; cp.s2_best = tt.s_best[1]; cp.s3_best = tt.s_best[2];
      }
      s.cars.push_back(cp);

      const double prog = cp.lap * C + cp.s;
      progress.push_back(prog);
      if (prog > leader_prog) { leader_prog = prog; leader_index = i; }
    }

    // Compute gaps relative to leader
    double leader_speed = 1.0; // avoid division by zero
    if (n > 0) {
      if (const auto* leader = sim.car_by_index(leader_index)) {
        leader_speed = std::max(1.0, leader->speed_mps);
      }
    }
    for (std::size_t i = 0; i < s.cars.size(); ++i) {
      double gap_m = leader_prog - progress[i];
      if (gap_m < 0.0) gap_m = 0.0;
      s.cars[i].gap_to_leader_m = gap_m;
      s.cars[i].gap_to_leader_s = gap_m / leader_speed;
    }

    // Back-compat fill primary from car id 0 (if present) or index 0
    if (!s.cars.empty()) {
      const CarPose* primary = nullptr;
      for (const auto& c : s.cars) if (c.id == 0u) { primary = &c; break; }
      if (!primary) primary = &s.cars.front();
      s.x = primary->x; s.y = primary->y; s.heading_rad = primary->heading_rad;
      s.s = primary->s; s.lap = primary->lap;
    }

    buffer_.publish(s);

    next += tick_ns;
    std::this_thread::sleep_until(next);
  }
}

} // namespace f1tm
