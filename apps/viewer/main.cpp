#include <raylib.h>
#include <cmath>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

#include <f1tm/sim.hpp>
#include <f1tm/snap.hpp>
#include <f1tm/snap_buffer.hpp>
#include <f1tm/interp.hpp>

using namespace f1tm;

static Vector2 worldToScreen(double x, double y, int width, int height, float scale) {
  const float cx = width * 0.5f, cy = height * 0.5f;
  return { cx + float(x * scale), cy - float(y * scale) };
}

static const char* warpLabel(double w) {
  if (w == 0.0) return "Paused";
  if (w == 0.25) return "0.25x";
  if (w == 0.5)  return "0.5x";
  if (w == 1.0)  return "1x";
  if (w == 2.0)  return "2x";
  if (w == 4.0)  return "4x";
  return "custom";
}

int main() {
  const int W = 1024, H = 768;
  InitWindow(W, H, "F1TM Vertical Slice - Client/Server (2 threads, time warp)");
  SetTargetFPS(144);

  // --- Shared buffer and controls
  SnapshotBuffer buffer;
  std::atomic<bool>   running{true};
  std::atomic<double> time_scale{1.0};   // 0.0 = paused
  std::atomic<double> interp_delay{0.050}; // 50 ms render delay

  // --- Server thread (fixed cadence; warp scales dt applied to sim)
  std::thread server([&]{
    SimServer sim;
    sim.track.center_x = 0.0;
    sim.track.center_y = 0.0;
    sim.track.radius_m = 120.0;
    sim.car.speed_mps  = 70.0;

    using clock = std::chrono::steady_clock;
    const double base_dt = 1.0 / 240.0; // 240 Hz wall-clock cadence
    const auto   tick_ns = std::chrono::nanoseconds((long long)(base_dt * 1e9));
    auto next = clock::now();
    double sim_time = 0.0;
    uint64_t tick = 0;

    while (running.load(std::memory_order_relaxed)) {
      const double warp = time_scale.load(std::memory_order_relaxed);
      const double dt_eff = base_dt * (warp < 0.0 ? 0.0 : warp);

      if (dt_eff > 0.0) {
        sim.step(dt_eff);
        sim_time += dt_eff;
        ++tick;
      } else {
        // Even when paused, bump tick so clients know they’re receiving fresh snapshots
        ++tick;
      }

      // Publish snapshot
      double x, y, heading;
      sim.sample_pose(x, y, heading);
      SimSnapshot s{};
      s.x = x; s.y = y; s.heading_rad = heading;
      s.sim_time = sim_time; s.s = sim.car.s; s.lap = sim.car.laps; s.tick = tick;
      buffer.publish(s);

      next += tick_ns;
      std::this_thread::sleep_until(next);
    }
  });

  // --- Client state
  float scale_px_per_m = 2.0f;
  SimSnapshot snap{};              // latest raw snapshot
  uint64_t cursor = 0;
  InterpBuffer ibuf;               // interpolation buffer

  while (!WindowShouldClose()) {
    // Inputs: warp & zoom
    if (IsKeyPressed(KEY_SPACE))   time_scale.store( time_scale.load()==0.0 ? 1.0 : 0.0 );
    if (IsKeyPressed(KEY_ONE))     time_scale.store(0.25);
    if (IsKeyPressed(KEY_TWO))     time_scale.store(0.5);
    if (IsKeyPressed(KEY_THREE))   time_scale.store(1.0);
    if (IsKeyPressed(KEY_FOUR))    time_scale.store(2.0);
    if (IsKeyPressed(KEY_FIVE))    time_scale.store(4.0);

    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_KP_ADD))      scale_px_per_m *= 1.01f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_KP_SUBTRACT)) scale_px_per_m *= 0.99f;

    // Pull new snapshots; feed interpolation buffer
    while (buffer.try_consume_latest(cursor, snap)) {
      ibuf.push(snap);
    }

    // Target render time: slightly behind latest for robust interpolation
    SimSnapshot draw{};
    const double target = ibuf.latest_time() - interp_delay.load();
    if (!ibuf.sample(target, draw)) draw = snap;

    // Render
    BeginDrawing();
    ClearBackground(Color{18,18,22,255});

    // Track
    Vector2 center = worldToScreen(0.0, 0.0, W, H, scale_px_per_m);
    float r_px = 120.0f * scale_px_per_m;
    DrawCircleLinesV(center, r_px, Color{80,80,90,255});
    DrawCircleLinesV(center, r_px - 8.0f, Color{50,50,58,255});
    DrawCircleLinesV(center, r_px + 8.0f, Color{50,50,58,255});

    // Car (interpolated pose)
    Vector2 carPos = worldToScreen(draw.x, draw.y, W, H, scale_px_per_m);
    float len = 12.0f, wid = 6.0f;
    float c = std::cos(float(draw.heading_rad)), s = std::sin(float(draw.heading_rad));
    Vector2 nose  = { carPos.x + c*len,          carPos.y - s*len };
    Vector2 tailL = { carPos.x - c*len + s*wid,  carPos.y + s*len + c*wid };
    Vector2 tailR = { carPos.x - c*len - s*wid,  carPos.y + s*len - c*wid };
    DrawTriangle(nose, tailL, tailR, Color{220, 80, 80, 255});
    DrawCircleV(carPos, 2.0f, Color{255,255,255,200});

    // HUD
    const double warp = time_scale.load();
    DrawText(TextFormat("lap=%llu sim=%.2fs warp=%s",
                        (unsigned long long)draw.lap,
                        draw.sim_time,
                        warpLabel(warp)),
             20, 20, 20, Color{200,200,210,255});
    DrawText("Space: Pause/Resume   1..5: 0.25x 0.5x 1x 2x 4x   W/S or +/-: Zoom   Esc: Quit",
             20, 46, 16, Color{140,140,150,255});
    EndDrawing();
  }

  // shutdown
  running.store(false);
  if (server.joinable()) server.join();
  CloseWindow();
  return 0;
}
