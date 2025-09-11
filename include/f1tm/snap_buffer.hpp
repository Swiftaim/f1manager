#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <chrono>
#include <f1tm/snap.hpp>

namespace f1tm {

// Minimal, safe single-producer single-consumer snapshot buffer.
// Writer calls publish(); Reader calls try_consume_latest() or wait_for_new().
class SnapshotBuffer {
 public:
  void publish(const SimSnapshot& s) {
    {
      std::lock_guard<std::mutex> g(mu_);
      latest_ = s;
      seq_++;
    }
    cv_.notify_all();
  }

  // Non-blocking: returns true if a newer snapshot than 'cursor' was copied into 'out'.
  bool try_consume_latest(uint64_t& cursor, SimSnapshot& out) {
    uint64_t current = seq_.load(std::memory_order_acquire);
    if (current == cursor) return false;          // no new data
    {
      std::lock_guard<std::mutex> g(mu_);
      out = latest_;                              // copy small POD
      cursor = seq_.load(std::memory_order_relaxed);
    }
    return true;
  }

  // Blocking wait (with timeout). Returns true if new snapshot copied into 'out'.
  template <class Rep, class Period>
  bool wait_for_new(uint64_t& cursor, SimSnapshot& out,
                    const std::chrono::duration<Rep,Period>& timeout) {
    std::unique_lock<std::mutex> lk(mu_);
    bool got = cv_.wait_for(lk, timeout, [&]{ return seq_.load() != cursor; });
    if (!got) return false;
    out = latest_;
    cursor = seq_.load(std::memory_order_relaxed);
    return true;
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  SimSnapshot latest_{};
  std::atomic<uint64_t> seq_{0};
};

} // namespace f1tm
