#pragma once
#include <cstdint>
#include <f1tm/interp.hpp>
#include <f1tm/snap.hpp>

namespace f1tm {

class SimRunner;

// RAII application that renders the latest snapshots and HUD.
class ViewerApp {
public:
  explicit ViewerApp(SimRunner& sim);
  int run(); // returns 0 on normal exit

private:
  // Input & data flow
  void process_input_();
  void pump_snapshots_();
  // Rendering
  void render_frame_();
  void draw_track_(float scale_px_per_m);
  void draw_hud_(const SimSnapshot& draw);
      void draw_dashboard_(const SimSnapshot& draw);

  // Helpers
  struct Vec2f { float x; float y; };
  Vec2f worldToScreen_(double x, double y, float scale) const;

  // Dependencies
  SimRunner& sim_;
  // Client-side interpolation
  InterpBuffer ibuf_{};
  SimSnapshot last_snap_{};
  std::uint64_t cursor_{0};

  // UI state
  float  scale_px_per_m_{2.0f};
  double interp_delay_{0.050};
  // Camera pan (meters)
  float pan_x_m_{0.0f};
  float pan_y_m_{-100.0f};

  // N-cycle for quick reseed (1,2,4,8)
  int n_cycle_idx_{3}; // starts at 8 by default
};

} // namespace f1tm
