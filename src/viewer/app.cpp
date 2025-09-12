#include <raylib.h>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <cstdio>

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

  // Toggle N cars: cycle 1 -> 2 -> 4 -> 8 -> 1
  if (IsKeyPressed(KEY_N)) {
    static const int N_CYCLE[4] = {1,2,4,8};
    n_cycle_idx_ = (n_cycle_idx_ + 1) % 4;
    sim_.request_reseed(N_CYCLE[n_cycle_idx_]);
  }

  // Toggle Track Preset
  if (IsKeyPressed(KEY_T)) {
    auto p = sim_.current_preset();
    int next = (static_cast<int>(p) + 1) % static_cast<int>(TrackPreset::Count);
    sim_.request_track_preset(static_cast<TrackPreset>(next));
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
    DrawCircleV(pos, 2.0f, Color{255,255,255,200});
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
  const int pad = 8;
  const int x0 = 20, y0 = 80;
  const int box_w = 620;
  const int box_h = pad*2 + row_h*(int(cars.size()) + 2);

  DrawRectangle(x0 - 6, y0 - 6, box_w + 12, box_h + 12, Color{0,0,0,80});
  DrawRectangle(x0, y0, box_w, box_h, Color{24,24,28,220});
  DrawLine(x0, y0 + pad + row_h, x0 + box_w, y0 + pad + row_h, Color{60,60,70,255});

  DrawText("Pos  ID   Lap    Gap     Last       Best      S1       S2       S3",
           x0 + pad, y0 + pad - 2, 16, Color{220,220,230,255});

  int y = y0 + pad + row_h + 2;
  int pos = 1;
  char buf_last[32], buf_best[32], buf_id[16], buf_lap[16];
  char buf_gap[32], buf_s1[32], buf_s2[32], buf_s3[32];
  for (const auto& c : cars) {
    fmt_time(c.last_lap_time, buf_last, sizeof(buf_last));
    fmt_time(c.best_lap_time, buf_best, sizeof(buf_best));
    fmt_gap(c.gap_to_leader_s, buf_gap, sizeof(buf_gap));
    fmt_time(c.s1_last, buf_s1, sizeof(buf_s1));
    fmt_time(c.s2_last, buf_s2, sizeof(buf_s2));
    fmt_time(c.s3_last, buf_s3, sizeof(buf_s3));
    std::snprintf(buf_id,  sizeof(buf_id),  "%u", (unsigned)c.id);
    std::snprintf(buf_lap, sizeof(buf_lap), "%llu", (unsigned long long)c.lap);

    Color posCol = (pos==1) ? Color{255,215,0,255}
                  : (pos==2) ? Color{192,192,192,255}
                  : (pos==3) ? Color{205,127,50,255}
                             : Color{200,200,210,255};

    DrawText(TextFormat("%2d", pos), x0 + pad + 0,   y, 16, posCol);
    DrawRectangle(x0 + pad + 28, y+2, 10, 10, colorFor(c.id));
    DrawText(buf_id,                    x0 + pad + 42,  y, 16, colorFor(c.id));
    DrawText(buf_lap,                   x0 + pad + 92,  y, 16, Color{200,200,210,255});
    DrawText(buf_gap,                   x0 + pad + 148, y, 16, Color{200,200,210,255});
    DrawText(buf_last,                  x0 + pad + 210, y, 16, Color{200,200,210,255});
    DrawText(buf_best,                  x0 + pad + 300, y, 16, Color{200,200,210,255});
    DrawText(buf_s1,                    x0 + pad + 392, y, 16, Color{200,200,210,255});
    DrawText(buf_s2,                    x0 + pad + 472, y, 16, Color{200,200,210,255});
    DrawText(buf_s3,                    x0 + pad + 552, y, 16, Color{200,200,210,255});
    y += row_h;
    ++pos;
  }
}

void ViewerApp::draw_hud_(const SimSnapshot& draw) {
  const double warp = sim_.time_scale.load();
  const char* preset = sim_.preset_name();
  DrawText(TextFormat("track=%s  cars=%d  lap=%llu  sim=%.2fs  warp=%s (N cars, T track, C center)",
                      preset,
                      (int)draw.cars.size(),
                      (unsigned long long)draw.lap,
                      draw.sim_time,
                      warpLabel(warp)),
           20, 20, 20, Color{220,235,220,255});
  DrawText("Space: Pause/Resume   1..5: 0.25x 0.5x 1x 2x 4x   W/S or +/-: Zoom   Arrow keys: Pan   N: Cars   T: Track   C: Center   Esc: Quit",
           20, 46, 16, Color{190,205,190,255});
}

} // namespace f1tm
