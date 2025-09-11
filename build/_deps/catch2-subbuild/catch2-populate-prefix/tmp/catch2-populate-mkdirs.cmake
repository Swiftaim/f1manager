# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "E:/Code/cpp/RaceManager/build/_deps/catch2-src")
  file(MAKE_DIRECTORY "E:/Code/cpp/RaceManager/build/_deps/catch2-src")
endif()
file(MAKE_DIRECTORY
  "E:/Code/cpp/RaceManager/build/_deps/catch2-build"
  "E:/Code/cpp/RaceManager/build/_deps/catch2-subbuild/catch2-populate-prefix"
  "E:/Code/cpp/RaceManager/build/_deps/catch2-subbuild/catch2-populate-prefix/tmp"
  "E:/Code/cpp/RaceManager/build/_deps/catch2-subbuild/catch2-populate-prefix/src/catch2-populate-stamp"
  "E:/Code/cpp/RaceManager/build/_deps/catch2-subbuild/catch2-populate-prefix/src"
  "E:/Code/cpp/RaceManager/build/_deps/catch2-subbuild/catch2-populate-prefix/src/catch2-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/Code/cpp/RaceManager/build/_deps/catch2-subbuild/catch2-populate-prefix/src/catch2-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/Code/cpp/RaceManager/build/_deps/catch2-subbuild/catch2-populate-prefix/src/catch2-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
