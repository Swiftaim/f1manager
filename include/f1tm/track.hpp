#pragma once
#include <optional>
#include <string>
#include <vector>
#include <istream>
#include <f1tm/pit.hpp>

namespace f1tm {

struct Track {
  std::string key;          // e.g., "Bahrain"
  double pit_stationary_s;  // seconds
  double pit_lane_delta_s;  // seconds
  double sc_lane_factor;    // 0..1
  double vsc_lane_factor;   // 0..1
};

// Built-in tiny catalog (default/fallback).
const std::vector<Track>& track_catalog();

// Lookup helpers
std::optional<Track> track_by_key(const std::string& key);

// Lookup within a specific catalog (e.g., CSV-loaded)
std::optional<Track> track_by_key_in(const std::vector<Track>& cat, const std::string& key);

// Stream-based CSV loader (test-friendly; no filesystem required).
// Accepts an optional header row; ignores lines starting with '#' and blank lines.
// Whitespace around fields is trimmed. Invalid rows are skipped.
std::vector<Track> track_catalog_from_csv_stream(std::istream& in);

// Filesystem wrapper; returns nullopt if file cannot be opened.
std::optional<std::vector<Track>> load_track_catalog_csv(const std::string& path);

// Convenience: derive PitParams from a Track.
inline PitParams track_pit_params(const Track& t) {
  return PitParams{t.pit_stationary_s, t.pit_lane_delta_s};
}

} // namespace f1tm
