#include <raylib.h>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <ctime>

#include <f1tm/viewer/app.hpp>
#include <f1tm/sim_runner.hpp>
#include <f1tm/snap_buffer.hpp>
#include <f1tm/track_geom.hpp> // kPI, TrackPath

namespace f1tm {

namespace {

// radians -> degrees using our kPI constant
static constexpr double kRadToDeg = 180.0 / kPI;

static const char* warpLabel(double w) {
  if (w == 0.0)  return "Paused";
  if (w == 0.25) return "0.25x";
  if (w == 0.5)  return "0.5x";
  if (w == 1.0)  return "1x";
  if (w == 2.0)  return "2x";
  if (w == 4.0)  return "4x";
  return "custom";
}

// High-contrast palette; assigned per CarId (stable during session).
static Color colorFor(CarId id) {
  static const Color PAL[] = {
    {231, 76, 60, 255},   // red
    {52, 152, 219, 255},  // blue
    {46, 204, 113, 255},  // green
    {241, 196, 15, 255},  // yellow
    {155, 89, 182, 255},  // purple
    {26, 188, 156, 255},  // teal
    {230, 126, 34, 255},  // orange
    {236, 112, 99, 255},  // salmon
    {39, 174, 96, 255},   // dark green
    {52, 73, 94, 255},    // slate
    {127, 140, 141, 255}, // gray
    {241, 90, 36, 255},   // orange-red
    {0, 152, 117, 255},   // sea green
    {91, 44, 111, 255},   // deep purple
    {142, 68, 173, 255},  // amethyst
    {33, 97, 140, 255},   // steel blue
  };
  static std::unordered_map<CarId, int> idx;
  auto it = idx.find(id);
  if (it == idx.end()) {
    int assigned = static_cast<int>(idx.size()) % static_cast<int>(sizeof(PAL)/sizeof(PAL[0]));
    it = idx.emplace(id, assigned).first;
  }
  return PAL[it->second];
}

static float length2f(Vector2 v) { return std::sqrt(v.x*v.x + v.y*v.y); }

// Shoelace sign (CCW positive, CW negative)
static float polygon_area_sign(const std::vector<Vec2>& pts) {
  double A = 0.0;
  for (size_t i = 0; i + 1 < pts.size(); ++i) {
    A += pts[i].x * pts[i+1].y - pts[i+1].x * pts[i].y;
  }
  return (A >= 0.0) ? +1.0f : -1.0f;
}

// Time formatting helpers
static void fmt_time(double s, char* out, int cap) {
  if (cap <= 0 || out == nullptr) return;
  if (s < 0.0 || !std::isfinite(s)) { std::snprintf(out, (size_t)cap, "%s", "--"); return; }
  int minutes = (int)(s / 60.0);
  double rem  = s - minutes * 60.0;
  int secs    = (int)rem;
  int ms      = (int)((rem - secs) * 1000.0 + 0.5);
  if (minutes > 0) std::snprintf(out, (size_t)cap, "%d:%02d.%03d", minutes, secs, ms);
  else             std::snprintf(out, (size_t)cap, "%d.%03d", secs, ms);
}
static void fmt_gap(double s, char* out, int cap) {
  if (cap <= 0 || out == nullptr) return;
  if (s < 0.0) { std::snprintf(out, (size_t)cap, "%s", "--"); return; }
  std::snprintf(out, (size_t)cap, "+%.3f", s);
}

// --- HUD layout (keep in sync with draw_hud_) ---
static constexpr int kHUD_LINE1_Y    = 20;  // size 20
static constexpr int kHUD_LINE2_Y    = 46;  // size 18
static constexpr int kHUD_LINE3_Y    = 72;  // size 14
static constexpr int kHUD_BOTTOM_PAD = 24;  // extra breathing room below HUD

// ---------- Simple race control (viewer-side) ----------

enum class RaceMode { Laps, Duration };
struct RaceConfig {
  RaceMode mode{RaceMode::Laps};
  int      target_laps{5};
  double   target_seconds{180.0};
};
struct RaceState {
  bool   active{true};
  bool   finished{false};
  double finish_sim_time{0.0};
  std::string saved_json_path;
  std::string saved_csv_path;
  // snapshot of order at finish
  std::vector<CarPose> final_order;
};
static RaceConfig g_race_cfg{};
static RaceState  g_race_state{};

static std::string timestamp_yyyyMMdd_HHmmss_() {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
  return std::string(buf);
}

static void save_results_(const std::vector<CarPose>& ordered,
                          double sim_time,
                          const RaceConfig& cfg,
                          double track_len,
                          const char* track_name,
                          std::string& out_json,
                          std::string& out_csv) {
  if (ordered.empty()) return;
  // Leader progress
  auto progress = [&](const CarPose& c){ return c.lap * track_len + c.s; };
  const double leader_prog = progress(ordered.front());
  // Approximate leader speed from best lap if available
  double leader_speed = 1.0;
  if (ordered.front().best_lap_time > 0.0) {
    leader_speed = std::max(1.0, track_len / ordered.front().best_lap_time);
  }

  const std::string ts = timestamp_yyyyMMdd_HHmmss_();
  out_json = "race_results_" + ts + ".json";
  out_csv  = "race_results_" + ts + ".csv";

  // JSON (simple manual emitter)
  {
    std::ofstream jf(out_json, std::ios::binary);
    jf << "{\n";
    jf << "  \"track\": \"" << (track_name ? track_name : "Unknown") << "\",\n";
    jf << "  \"mode\": \"" << (cfg.mode==RaceMode::Laps ? "laps" : "duration") << "\",\n";
    jf << "  \"target\": " << (cfg.mode==RaceMode::Laps ? cfg.target_laps : (int)cfg.target_seconds) << ",\n";
    jf << "  \"finish_time\": " << sim_time << ",\n";
    jf << "  \"entries\": [\n";
    for (size_t i = 0; i < ordered.size(); ++i) {
      const auto& c = ordered[i];
      double gap_m = leader_prog - progress(c);
      if (gap_m < 0.0) gap_m = 0.0;
      double gap_s = gap_m / leader_speed;
      jf << "    {"
         << "\"pos\":" << (i+1)
         << ",\"id\":" << (unsigned)c.id
         << ",\"laps\":" << (unsigned long long)c.lap
         << ",\"best_lap\":" << c.best_lap_time
         << ",\"s1_best\":" << c.s1_best
         << ",\"s2_best\":" << c.s2_best
         << ",\"s3_best\":" << c.s3_best
         << ",\"gap_s\":" << (i==0?0.0:gap_s)
         << "}" << (i+1<ordered.size()? ",":"") << "\n";
    }
    jf << "  ]\n}\n";
  }

  // CSV
  {
    std::ofstream cf(out_csv, std::ios::binary);
    cf << "pos,id,laps,best_lap,s1_best,s2_best,s3_best,gap_s\n";
    for (size_t i = 0; i < ordered.size(); ++i) {
      const auto& c = ordered[i];
      double gap_m = leader_prog - progress(c);
      if (gap_m < 0.0) gap_m = 0.0;
      double gap_s = gap_m / leader_speed;
      cf << (i+1) << ","
         << (unsigned)c.id << ","
         << (unsigned long long)c.lap << ","
         << c.best_lap_time << ","
         << c.s1_best << ","
         << c.s2_best << ","
         << c.s3_best << ","
         << (i==0?0.0:gap_s) << "\n";
    }
  }
}

// Called every frame; decides finish conditions and persists results exactly once.
static void race_update_(const SimSnapshot& draw, const SimRunner& sim) {
  if (!g_race_state.active || g_race_state.finished) return;

  const double C = sim.track_path().length();
  // Find leader and compute finish condition
  auto cars = draw.cars;
  if (cars.empty()) return;

  std::sort(cars.begin(), cars.end(), [](const CarPose& a, const CarPose& b){
    if (a.lap != b.lap) return a.lap > b.lap;
    return a.s > b.s;
  });

  bool should_finish = false;
  if (g_race_cfg.mode == RaceMode::Laps) {
    // Finish when leader reaches target laps
    if ((int)cars.front().lap >= g_race_cfg.target_laps) should_finish = true;
  } else {
    if (draw.sim_time >= g_race_cfg.target_seconds) should_finish = true;
  }

  if (should_finish) {
    g_race_state.finished = true;
    g_race_state.finish_sim_time = draw.sim_time;
    g_race_state.final_order = cars;
    save_results_(g_race_state.final_order,
                  g_race_state.finish_sim_time,
                  g_race_cfg,
                  C,
                  sim.preset_name(),
                  g_race_state.saved_json_path,
                  g_race_state.saved_csv_path);
  }
}

static void race_input_() {
  // Toggle mode
  if (IsKeyPressed(KEY_M)) {
    g_race_cfg.mode = (g_race_cfg.mode == RaceMode::Laps) ? RaceMode::Duration : RaceMode::Laps;
  }
  // Adjust target
  if (IsKeyPressed(KEY_LEFT_BRACKET)) {
    if (g_race_cfg.mode == RaceMode::Laps) g_race_cfg.target_laps = std::max(1, g_race_cfg.target_laps - 1);
    else g_race_cfg.target_seconds = std::max(30.0, g_race_cfg.target_seconds - 30.0);
  }
  if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
    if (g_race_cfg.mode == RaceMode::Laps) g_race_cfg.target_laps += 1;
    else g_race_cfg.target_seconds += 30.0;
  }
  // Reset race
  if (IsKeyPressed(KEY_R)) {
    g_race_state = RaceState{}; // active=true, finished=false
  }
}

} // namespace

// ---- ViewerApp ----

ViewerApp::ViewerApp(SimRunner& sim) : sim_(sim) {}

ViewerApp::Vec2f ViewerApp::worldToScreen_(double x, double y, float scale) const {
  const float cx = GetScreenWidth()  * 0.5f + pan_x_m_ * scale;
  const float cy = GetScreenHeight() * 0.5f - pan_y_m_ * scale;
  return { cx + float(x * scale), cy - float(y * scale) };
}

int ViewerApp::run() {
  const int W = 1024, H = 768;
  InitWindow(W, H, "F1TM - Viewer");
  SetTargetFPS(144);

  while (!WindowShouldClose()) {
    process_input_();
    pump_snapshots_();
    render_frame_();
  }

  CloseWindow();
  return 0;
}

void ViewerApp::process_input_() {
  // Time warp controls
  if (IsKeyPressed(KEY_SPACE)) {
    double cur = sim_.time_scale.load();
    sim_.time_scale.store(cur == 0.0 ? 1.0 : 0.0);
  }
  if (IsKeyPressed(KEY_ONE))   sim_.time_scale.store(0.25);
  if (IsKeyPressed(KEY_TWO))   sim_.time_scale.store(0.5);
  if (IsKeyPressed(KEY_THREE)) sim_.time_scale.store(1.0);
  if (IsKeyPressed(KEY_FOUR))  sim_.time_scale.store(2.0);
  if (IsKeyPressed(KEY_FIVE))  sim_.time_scale.store(4.0);

  // Zoom
  if (IsKeyDown(KEY_W) || IsKeyDown(KEY_KP_ADD))      scale_px_per_m_ *= 1.01f;
  if (IsKeyDown(KEY_S) || IsKeyDown(KEY_KP_SUBTRACT)) scale_px_per_m_ *= 0.99f;

  // Camera pan
  const float pan_step = 0.6f; // meters per frame while key held
  if (IsKeyDown(KEY_LEFT))  pan_x_m_ -= pan_step;
  if (IsKeyDown(KEY_RIGHT)) pan_x_m_ += pan_step;
  if (IsKeyDown(KEY_UP))    pan_y_m_ += pan_step;
  if (IsKeyDown(KEY_DOWN))  pan_y_m_ -= pan_step;
  if (IsKeyPressed(KEY_C))  { pan_x_m_ = 0.0f; pan_y_m_ = -100.0f; } // default offset (track lower)

  // Race control hotkeys
  race_input_();

  // Toggle N cars: cycle 1 -> 2 -> 4 -> 8 -> 1
  if (IsKeyPressed(KEY_N)) {
    static const int N_CYCLE[4] = {1,2,4,8};
    n_cycle_idx_ = (n_cycle_idx_ + 1) % 4;
    sim_.request_reseed(N_CYCLE[n_cycle_idx_]);
    // reset race because field changed
    g_race_state = RaceState{};
  }

  // Toggle Track Preset
  if (IsKeyPressed(KEY_T)) {
    auto p = sim_.current_preset();
    int next = (static_cast<int>(p) + 1) % static_cast<int>(TrackPreset::Count);
    sim_.request_track_preset(static_cast<TrackPreset>(next));
    // reset race because track changed
    g_race_state = RaceState{};
  }
}

void ViewerApp::pump_snapshots_() {
  // Pull any new snapshots; store into interpolation buffer
  auto& buf = sim_.buffer();
  while (buf.try_consume_latest(cursor_, last_snap_)) {
    ibuf_.push(last_snap_);
  }
}

void ViewerApp::render_frame_() {
  // Resolve draw snapshot (slightly behind latest for interpolation)
  SimSnapshot draw = last_snap_;
  const double target = ibuf_.latest_time() - interp_delay_;
  (void)ibuf_.sample(target, draw);

  // Update race state & save results when finishing
  race_update_(draw, sim_);

  BeginDrawing();
  // Grass background
  ClearBackground(Color{30, 60, 30, 255});

  draw_track_(scale_px_per_m_);

  // Draw each car
  for (const auto& car : draw.cars) {
    Vector2 pos = { worldToScreen_(car.x, car.y, scale_px_per_m_).x,
                    worldToScreen_(car.x, car.y, scale_px_per_m_).y };
    float len = 12.0f, wid = 6.0f;
    float c = std::cos(float(car.heading_rad)), s = std::sin(float(car.heading_rad));
    Vector2 nose  = { pos.x + c*len,          pos.y - s*len };
    Vector2 tailL = { pos.x - c*len + s*wid,  pos.y + s*len + c*wid };
    Vector2 tailR = { pos.x - c*len - s*wid,  pos.y + s*len - c*wid };
    DrawTriangle(nose, tailL, tailR, colorFor(car.id));
    DrawCircleV(pos, 3.0f, colorFor(car.id)); // colored marker at center
  }

  draw_dashboard_(draw);
  draw_hud_(draw);
  EndDrawing();
}

void ViewerApp::draw_track_(float scale_px_per_m) {
  const auto& path = sim_.track_path();
  const auto& pts = path.points();
  if (pts.size() < 2) return;

  const float width_m = 12.0f;
  const float half_w_px = 0.5f * width_m * scale_px_per_m;

  // Asphalt ribbon (thick segment lines)
  for (std::size_t i = 1; i < pts.size(); ++i) {
    auto a = worldToScreen_(pts[i-1].x, pts[i-1].y, scale_px_per_m);
    auto b = worldToScreen_(pts[i].x,   pts[i].y,   scale_px_per_m);
    DrawLineEx({a.x,a.y}, {b.x,b.y}, half_w_px*2.0f, Color{40,40,46,255});
  }
  // Edges
  for (std::size_t i = 1; i < pts.size(); ++i) {
    auto a = worldToScreen_(pts[i-1].x, pts[i-1].y, scale_px_per_m);
    auto b = worldToScreen_(pts[i].x,   pts[i].y,   scale_px_per_m);
    DrawLineEx({a.x,a.y}, {b.x,b.y}, 2.0f, Color{30,30,34,255});
  }

  // Kerbs along inner edge (alternate red/white short dashes)
  const float orient = polygon_area_sign(pts); // +1 for CCW
  const float kerb_dash_px = 14.0f;
  const float kerb_thick_px = 6.0f;
  bool red = true;
  for (std::size_t i = 1; i < pts.size(); ++i) {
    auto a = worldToScreen_(pts[i-1].x, pts[i-1].y, scale_px_per_m);
    auto b = worldToScreen_(pts[i].x,   pts[i].y,   scale_px_per_m);
    Vector2 ab = { b.x - a.x, b.y - a.y };
    const float len = length2f(ab);
    if (len < 1.0f) continue;
    // tangent + normal
    Vector2 t = { ab.x / len, ab.y / len };
    Vector2 n = { -t.y * orient, t.x * orient }; // inner edge
    const Vector2 inner_a = { a.x + n.x * (half_w_px - kerb_thick_px*0.5f),
                              a.y + n.y * (half_w_px - kerb_thick_px*0.5f) };
    float consumed = 0.0f;
    while (consumed < len) {
      float dash = std::min(kerb_dash_px, len - consumed);
      Vector2 p0 = { inner_a.x + t.x * consumed,           inner_a.y + t.y * consumed };
      Vector2 p1 = { inner_a.x + t.x * (consumed + dash),  inner_a.y + t.y * (consumed + dash) };
      DrawLineEx(p0, p1, kerb_thick_px, red ? Color{200,70,70,255} : Color{235,235,235,255});
      consumed += dash;
      red = !red;
    }
  }

  // Start/finish checker (at segment 0->1)
  if (pts.size() >= 2) {
    auto a = worldToScreen_(pts[0].x, pts[0].y, scale_px_per_m);
    auto b = worldToScreen_(pts[1].x, pts[1].y, scale_px_per_m);
    Vector2 ab = { b.x - a.x, b.y - a.y };
    const float len = length2f(ab);
    if (len > 0.1f) {
      Vector2 t = { ab.x / len, ab.y / len };
      Vector2 n = { -t.y, t.x };
      const int squares = 10;
      for (int i = 0; i < squares; ++i) {
        Color c = (i % 2 == 0) ? Color{240,240,240,255} : Color{20,20,22,255};
        float off = -half_w_px + (2.0f*half_w_px) * ((i + 0.5f) / squares);
        Vector2 p0 = { a.x + n.x * off, a.y + n.y * off };
        Vector2 p1 = { p0.x + t.x * 8.0f, p0.y + t.y * 8.0f };
        DrawLineEx(p0, p1, 6.0f, c);
      }
    }
  }

  // Grid boxes for current car count (from latest snapshot)
  int car_count = (int)last_snap_.cars.size();
  const int rows = (car_count + 1) / 2;
  const float row_gap_m   = 9.0f;
  const float lane_gap_m  = 3.0f;   // off-pole further back
  const float box_len_m   = 4.0f;   // along tangent
  const float lane_off_m  = width_m * 0.25f; // lateral offset from centerline

  if (pts.size() >= 2) {
    // derive tangent at s=0 from pts[0]->pts[1]
    const double dx = (pts[1].x - pts[0].x), dy = (pts[1].y - pts[0].y);
    const double t = std::atan2(dy, dx);
    const double cos_t = std::cos(t), sin_t = std::sin(t);
    for (int row = 0; row < rows; ++row) {
      for (int lane = 0; lane < 2; ++lane) {
        const int idx = row * 2 + lane;
        if (idx >= car_count) break;
        const float back_m = row * row_gap_m + (lane == 1 ? lane_gap_m : 0.0f);
        const double px = pts[0].x - cos_t * back_m;
        const double py = pts[0].y - sin_t * back_m;
        const double nx = -sin_t, ny =  cos_t;
        const double off = (lane == 0 ? -lane_off_m : lane_off_m);
        auto boxc = worldToScreen_(px + nx*off, py + ny*off, scale_px_per_m);
        const float angle_deg = float((t + kPI * 0.5) * kRadToDeg);
        Rectangle box {
          boxc.x,
          boxc.y,
          width_m * 0.7f * scale_px_per_m,   // across track
          box_len_m * scale_px_per_m         // along track
        };
        DrawRectanglePro(box, {box.width*0.5f, box.height*0.5f}, angle_deg, Color{255,255,255,30});
      }
    }
  }
}

void ViewerApp::draw_dashboard_(const SimSnapshot& draw) {
  // Sorted by race position (lap desc, s desc)
  std::vector<CarPose> cars = draw.cars;
  std::sort(cars.begin(), cars.end(), [](const CarPose& a, const CarPose& b){
    if (a.lap != b.lap) return a.lap > b.lap;
    return a.s > b.s;
  });

  const int row_h = 18;
  const int pad   = 8;
  const int x0    = 20;
  // NEW: place dashboard below the HUD + padding
  const int y0    = kHUD_LINE3_Y + /*line height*/ 14 + kHUD_BOTTOM_PAD; // 72 + 14 + 24 = 110
  const int box_w = 620;
  const int box_h = pad*2 + row_h*(int(cars.size()) + 2);

  // Panel
  DrawRectangle(x0 - 6, y0 - 6, box_w + 12, box_h + 12, Color{0,0,0,80});
  DrawRectangle(x0, y0, box_w, box_h, Color{24,24,28,220});
  DrawLine(x0, y0 + pad + row_h, x0 + box_w, y0 + pad + row_h, Color{60,60,70,255});

  // Column x-positions (match row draws below)
  const int X_POS  = x0 + pad + 0;
  const int X_ID   = x0 + pad + 42;
  const int X_LAP  = x0 + pad + 92;
  const int X_GAP  = x0 + pad + 148;
  const int X_LAST = x0 + pad + 210;
  const int X_BEST = x0 + pad + 300;
  const int X_S1   = x0 + pad + 392;
  const int X_S2   = x0 + pad + 472;
  const int X_S3   = x0 + pad + 552;

  // Headers (aligned to columns)
  const Color hdr = Color{220,220,230,255};
  DrawText("Pos",  X_POS,  y0 + pad - 2, 16, hdr);
  DrawText("ID",   X_ID,   y0 + pad - 2, 16, hdr);
  DrawText("Lap",  X_LAP,  y0 + pad - 2, 16, hdr);
  DrawText("Gap",  X_GAP,  y0 + pad - 2, 16, hdr);
  DrawText("Last", X_LAST, y0 + pad - 2, 16, hdr);
  DrawText("Best", X_BEST, y0 + pad - 2, 16, hdr);
  DrawText("S1",   X_S1,   y0 + pad - 2, 16, hdr);
  DrawText("S2",   X_S2,   y0 + pad - 2, 16, hdr);
  DrawText("S3",   X_S3,   y0 + pad - 2, 16, hdr);

  // --- Sector highlight pre-pass: find current lap-best (min) among cars ---
  double s1_min = -1.0, s2_min = -1.0, s3_min = -1.0;
  auto upd_min = [](double last, double& acc){
    if (last >= 0.0) acc = (acc < 0.0) ? last : std::min(acc, last);
  };
  for (const auto& c : cars) {
    upd_min(c.s1_last, s1_min);
    upd_min(c.s2_last, s2_min);
    upd_min(c.s3_last, s3_min);
  }

  // Sector cell colors
  const Color colDefault = Color{200,200,210,255};
  const Color colGreen   = Color{ 80,220,120,255};  // lap best
  const Color colPurple  = Color{180, 90,255,255};  // personal best
  auto sectorColor = [&](double last, double best, double lapMin)->Color {
    if (last < 0.0) return colDefault;
    const double eps = 1e-4;
    if (best   > 0.0 && std::fabs(last - best)   <= eps) return colPurple; // personal best
    if (lapMin > 0.0 && std::fabs(last - lapMin) <= eps) return colGreen;  // lap best
    return colDefault;
  };

  // Rows
  int y = y0 + pad + row_h + 2;
  int pos = 1;
  char buf_last[32], buf_best[32], buf_id[16], buf_lap[16];
  char buf_gap[32], buf_s1[32], buf_s2[32], buf_s3[32];

  for (const auto& c : cars) {
    fmt_time(c.last_lap_time, buf_last, sizeof(buf_last));
    fmt_time(c.best_lap_time, buf_best, sizeof(buf_best));
    fmt_gap (c.gap_to_leader_s, buf_gap, sizeof(buf_gap));
    fmt_time(c.s1_last, buf_s1, sizeof(buf_s1));
    fmt_time(c.s2_last, buf_s2, sizeof(buf_s2));
    fmt_time(c.s3_last, buf_s3, sizeof(buf_s3));
    std::snprintf(buf_id,  sizeof(buf_id),  "%u", (unsigned)c.id);
    std::snprintf(buf_lap, sizeof(buf_lap), "%llu", (unsigned long long)c.lap);

    Color posCol = (pos==1) ? Color{255,215,0,255}
                  : (pos==2) ? Color{192,192,192,255}
                  : (pos==3) ? Color{205,127,50,255}
                             : Color{200,200,210,255};

    DrawText(TextFormat("%2d", pos), X_POS, y, 16, posCol);
    DrawRectangle(X_ID - 14, y+2, 10, 10, colorFor(c.id)); // color swatch
    DrawText(buf_id,  X_ID,   y, 16, colorFor(c.id));
    DrawText(buf_lap, X_LAP,  y, 16, colDefault);
    DrawText(buf_gap, X_GAP,  y, 16, colDefault);
    DrawText(buf_last,X_LAST, y, 16, colDefault);
    DrawText(buf_best,X_BEST, y, 16, colDefault);
    DrawText(buf_s1,  X_S1,   y, 16, sectorColor(c.s1_last, c.s1_best, s1_min));
    DrawText(buf_s2,  X_S2,   y, 16, sectorColor(c.s2_last, c.s2_best, s2_min));
    DrawText(buf_s3,  X_S3,   y, 16, sectorColor(c.s3_last, c.s3_best, s3_min));

    y += row_h;
    ++pos;
  }
}

void ViewerApp::draw_hud_(const SimSnapshot& draw) {
  const double warp = sim_.time_scale.load();
  const char* preset = sim_.preset_name();

  // Race status line
  char race_line[256];
  if (g_race_cfg.mode == RaceMode::Laps) {
    std::snprintf(race_line, sizeof(race_line),
      "Race: Laps %d  —  %s%s",
      g_race_cfg.target_laps,
      g_race_state.finished ? "Finished" : "Active",
      g_race_state.finished ? (g_race_state.saved_json_path.empty() ? "" :
        (std::string(" (saved: ") + g_race_state.saved_json_path + ", " + g_race_state.saved_csv_path + ")").c_str()) : ""
    );
  } else {
    char dur[32]; fmt_time(g_race_cfg.target_seconds, dur, sizeof(dur));
    std::snprintf(race_line, sizeof(race_line),
      "Race: Time %s  —  %s%s",
      dur,
      g_race_state.finished ? "Finished" : "Active",
      g_race_state.finished ? (g_race_state.saved_json_path.empty() ? "" :
        (std::string(" (saved: ") + g_race_state.saved_json_path + ", " + g_race_state.saved_csv_path + ")").c_str()) : ""
    );
  }

  DrawText(TextFormat("track=%s  cars=%d  lap=%llu  sim=%.2fs  warp=%s",
                      preset,
                      (int)draw.cars.size(),
                      (unsigned long long)draw.lap,
                      draw.sim_time,
                      warpLabel(warp)),
           20, 20, 20, Color{220,235,220,255});

  DrawText(race_line, 20, 46, 18, Color{235,220,220,255});

  DrawText("Space: Pause/Resume | 1..5: 0.25x 0.5x 1x 2x 4x | W/S or +/-: Zoom | Arrows: Pan | N: Cars | T: Track | C: Center | M: Laps/Time | [ ]: Target | R: Reset",
           20, 72, 14, Color{190,205,190,255});
}

} // namespace f1tm
