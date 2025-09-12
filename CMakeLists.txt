cmake_minimum_required(VERSION 3.22)
project(f1tm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
  add_compile_options(/W4 /permissive- /EHsc /Zc:__cplusplus)
endif()

include(FetchContent)

# --- Raylib (graphics)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# Catch2
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.10.0 # latest known stable
)
FetchContent_MakeAvailable(catch2)

# Core library
add_library(f1tm_core
  src/stint.cpp
  src/pit.cpp
  src/race.cpp
  src/track.cpp
  src/events.cpp
  src/sim.cpp
)
target_include_directories(f1tm_core PUBLIC include)
target_compile_features(f1tm_core PUBLIC cxx_std_20)

# ---- App (viewer)

# If raylib isn't already available in your build, fetch it.
# (Safe to leave even if you've already added raylib elsewhere — it will no-op if TARGET raylib exists.)
if (NOT TARGET raylib)
  include(FetchContent)
  FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
  )
  FetchContent_MakeAvailable(raylib)
endif()

# App target with the refactored, clean structure.
# If you already created f1tm_app earlier, remove it and use this exact definition.
add_executable(f1tm_app
  # Entry point
  apps/viewer/main.cpp

  # Viewer UI (no raylib headers in public includes)
  src/viewer/app.cpp
  include/f1tm/viewer/app.hpp

  # Server thread owner (clean seam)
  src/sim_runner.cpp
  include/f1tm/sim_runner.hpp

  # Core sim (multi-car from M1)
  src/sim.cpp
  include/f1tm/sim.hpp

  # If you have separate sources for snapshot/interp, add them here:
  # src/snap_buffer.cpp
  # include/f1tm/snap_buffer.hpp
  # include/f1tm/snap.hpp
  # include/f1tm/interp.hpp
)

# C++ standard and includes
target_compile_features(f1tm_app PRIVATE cxx_std_20)
target_include_directories(f1tm_app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Warnings & MSVC hygiene
if (MSVC)
  target_compile_definitions(f1tm_app PRIVATE NOMINMAX)
  target_compile_options(f1tm_app PRIVATE /W4 /permissive-)
else()
  target_compile_options(f1tm_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Link raylib (it brings system libs on Windows)
target_link_libraries(f1tm_app PRIVATE raylib)

# Nice-to-have for VS debugging
if (WIN32)
  set_property(TARGET f1tm_app PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()




# Tests
add_subdirectory(tests)

enable_testing()
