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

# --- App (viewer)
add_executable(f1tm_app
  apps/viewer/main.cpp
)
target_link_libraries(f1tm_app PRIVATE f1tm_core raylib)
if (MSVC)
  target_compile_options(f1tm_app PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus)
endif()

# Tests
add_subdirectory(tests)

enable_testing()
