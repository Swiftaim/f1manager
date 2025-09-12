#pragma once
#include <atomic>
#include <cstdint>
#include <optional>

namespace f1tm {

// Single-producer single-consumer latest-only snapshot buffer.
template <class T>
class LatestBuffer {
public:
  void publish(const T& v) {
    // Copy into the slot then bump the sequence.
    data_ = v;
    seq_.fetch_add(1, std::memory_order_release);
  }

  // Try to consume if sequence advanced past cursor.
  bool try_consume_latest(std::uint64_t& cursor, T& out) const {
    const auto s = seq_.load(std::memory_order_acquire);
    if (s != cursor) {
      out = data_;
      cursor = s;
      return true;
    }
    return false;
  }
private:
  mutable T data_{};
  std::atomic<std::uint64_t> seq_{0};
};

using SnapshotBuffer = LatestBuffer<struct SimSnapshot>;

} // namespace f1tm
