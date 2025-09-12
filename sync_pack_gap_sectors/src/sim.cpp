#include <f1tm/sim.hpp>

namespace f1tm {

void SimServer::add_car(CarId id, double speed_mps, double s0, std::uint64_t laps0) {
  CarState cs;
  cs.id = id;
  cs.speed_mps = speed_mps;
  cs.s = s0;
  cs.laps = laps0;
  cars_.push_back(cs);
}

const CarState* SimServer::car_by_index(std::size_t idx) const {
  if (idx >= cars_.size()) return nullptr;
  return &cars_[idx];
}
CarState* SimServer::car_by_index(std::size_t idx) {
  if (idx >= cars_.size()) return nullptr;
  return &cars_[idx];
}

const CarState* SimServer::car_by_id(CarId id) const {
  for (const auto& c : cars_) if (c.id == id) return &c;
  return nullptr;
}
CarState* SimServer::car_by_id(CarId id) {
  for (auto& c : cars_) if (c.id == id) return &c;
  return nullptr;
}

double SimServer::track_length() const {
  if (use_path_ && path_.has_value() && !path_->empty()) return path_->length();
  return track.circumference_m();
}

void SimServer::step(double dt_sec) {
  const double C = track_length();
  if (C <= 0.0 || dt_sec <= 0.0) return;
  for (auto& c : cars_) {
    if (c.speed_mps <= 0.0) continue;
    c.s += c.speed_mps * dt_sec;
    while (c.s >= C) {
      c.s -= C;
      ++c.laps;
    }
  }
}

void SimServer::sample_pose(double& x, double& y, double& heading_rad) const {
  if (!cars_.empty()) {
    if (use_path_ && path_.has_value() && !path_->empty())
      path_->sample_pose(cars_[0].s, x, y, heading_rad);
    else
      s_to_pose_circle(track, cars_[0].s, x, y, heading_rad);
  } else {
    if (use_path_ && path_.has_value() && !path_->empty())
      path_->sample_pose(0.0, x, y, heading_rad);
    else
      s_to_pose_circle(track, 0.0, x, y, heading_rad);
  }
}

void SimServer::sample_pose_index(std::size_t idx, double& x, double& y, double& heading_rad) const {
  if (idx < cars_.size()) {
    if (use_path_ && path_.has_value() && !path_->empty())
      path_->sample_pose(cars_[idx].s, x, y, heading_rad);
    else
      s_to_pose_circle(track, cars_[idx].s, x, y, heading_rad);
  } else {
    if (use_path_ && path_.has_value() && !path_->empty())
      path_->sample_pose(0.0, x, y, heading_rad);
    else
      s_to_pose_circle(track, 0.0, x, y, heading_rad);
  }
}

bool SimServer::sample_pose_for(CarId id, double& x, double& y, double& heading_rad) const {
  for (const auto& c : cars_) if (c.id == id) {
    if (use_path_ && path_.has_value() && !path_->empty())
      path_->sample_pose(c.s, x, y, heading_rad);
    else
      s_to_pose_circle(track, c.s, x, y, heading_rad);
    return true;
  }
  if (use_path_ && path_.has_value() && !path_->empty())
    path_->sample_pose(0.0, x, y, heading_rad);
  else
    s_to_pose_circle(track, 0.0, x, y, heading_rad);
  return false;
}

} // namespace f1tm
