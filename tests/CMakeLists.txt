add_executable(f1tm_tests
  test_stint.cpp
  test_pit.cpp
  test_race.cpp
  test_track.cpp
  test_track_csv.cpp
  test_race_track.cpp
  test_events.cpp
  test_sim.cpp
  test_snap.cpp
  test_interp.cpp 
  test_timewarp.cpp
)

target_link_libraries(f1tm_tests
  PRIVATE
    f1tm_core
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(f1tm_tests)
