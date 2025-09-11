#include <f1tm/track.hpp>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cctype>

namespace f1tm {

static std::string trim(std::string s) {
  auto not_space = [](unsigned char c){ return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

static std::vector<std::string> split_csv_line(const std::string& line) {
  // Simple CSV: no quoted fields; we keep parser tiny on purpose.
  std::vector<std::string> cols;
  std::string cur;
  for (char c : line) {
    if (c == ',') { cols.push_back(trim(cur)); cur.clear(); }
    else { cur.push_back(c); }
  }
  cols.push_back(trim(cur));
  return cols;
}

static bool is_header_row(const std::vector<std::string>& cols) {
  if (cols.size() < 5) return false;
  // Very light heuristic
  return (cols[0] == "key" || cols[0] == "Key");
}

static double to_double_safe(const std::string& s, bool& ok) {
  try {
    size_t idx = 0;
    double v = std::stod(s, &idx);
    ok = idx == s.size();
    return v;
  } catch(...) {
    ok = false;
    return 0.0;
  }
}

static std::optional<Track> parse_track_row(const std::vector<std::string>& cols) {
  if (cols.size() < 5) return std::nullopt;
  const std::string key = cols[0];
  if (key.empty()) return std::nullopt;
  bool ok1, ok2, ok3, ok4;
  const double stat = to_double_safe(cols[1], ok1);
  const double lane = to_double_safe(cols[2], ok2);
  double sc  = to_double_safe(cols[3], ok3);
  double vsc = to_double_safe(cols[4], ok4);
  if (!(ok1 && ok2 && ok3 && ok4)) return std::nullopt;

  auto clamp01 = [](double x){ return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); };
  sc  = clamp01(sc);
  vsc = clamp01(vsc);

  return Track{key, stat < 0.0 ? 0.0 : stat, lane < 0.0 ? 0.0 : lane, sc, vsc};
}

static std::vector<Track> make_catalog_builtin() {
  return {
    {"Bahrain", 2.5, 17.0, 0.45, 0.75},
    {"Monaco",  2.5, 21.0, 0.40, 0.70},
  };
}

const std::vector<Track>& track_catalog() {
  static const std::vector<Track> cat = make_catalog_builtin();
  return cat;
}

std::optional<Track> track_by_key(const std::string& key) {
  const auto& cat = track_catalog();
  return track_by_key_in(cat, key);
}

std::optional<Track> track_by_key_in(const std::vector<Track>& cat, const std::string& key) {
  auto it = std::find_if(cat.begin(), cat.end(), [&](const Track& t){ return t.key == key; });
  if (it == cat.end()) return std::nullopt;
  return *it;
}

std::vector<Track> track_catalog_from_csv_stream(std::istream& in) {
  std::vector<Track> out;
  std::string line;
  bool header_consumed = false;

  while (std::getline(in, line)) {
    std::string raw = trim(line);
    if (raw.empty()) continue;
    if (!raw.empty() && raw[0] == '#') continue;

    auto cols = split_csv_line(raw);

    if (!header_consumed && is_header_row(cols)) {
      header_consumed = true;
      continue;
    }

    if (auto row = parse_track_row(cols); row.has_value()) {
      out.push_back(*row);
    }
  }
  return out;
}

std::optional<std::vector<Track>> load_track_catalog_csv(const std::string& path) {
  std::ifstream f(path);
  if (!f) return std::nullopt;
  return track_catalog_from_csv_stream(f);
}

} // namespace f1tm
