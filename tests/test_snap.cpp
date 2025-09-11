#include <catch2/catch_test_macros.hpp>
#include <f1tm/snap.hpp>
#include <f1tm/snap_buffer.hpp>

using namespace f1tm;

TEST_CASE("SnapshotBuffer publishes and consumes latest") {
  SnapshotBuffer buf;
  SimSnapshot s; s.tick = 1; s.x = 1.0;
  buf.publish(s);

  uint64_t cursor = 0;
  SimSnapshot out{};
  // Should see new data
  REQUIRE(buf.try_consume_latest(cursor, out));
  REQUIRE(out.tick == 1);
  // Second call without publish should return false
  REQUIRE_FALSE(buf.try_consume_latest(cursor, out));
}
