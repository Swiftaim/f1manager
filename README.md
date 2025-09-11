# F1 Team Manager Simulator (Dev Notes)

## Developer CLI: `cpp`

To simplify our TDD loop on Windows 11 + VS Code + CMake + MSVC, we use a tiny Python script `cpp.py` with a Windows shim `cpp.cmd`.

Place both files in the repo root, then you can run:

### One-time configure
```powershell
.\cpp configure
.\cpp build
.\cpp build --target f1tm_core
.\cpp build --target f1tm_tests

# Run all tests via ctest
.\cpp test

# Filter tests via regex
.\cpp test --regex StintModel

# Run tests directly (Catch2 args after --)
.\cpp test --direct -- --list-tests
.\cpp test --direct -- --list-tags

# Build + run a target
.\cpp run --target f1tm_tests --args "--list-tests"

# Or run explicit exe path
.\cpp run --path .\build\tests\Debug\f1tm_tests.exe --args "--list-tests"


.\cpp clean


.\cpp info


Defaults

Generator: Visual Studio 17 2022

Arch: x64

Build dir: ./build

Config: Debug (override with --config Release or env var CMAKE_CONFIG)

.\cpp test --direct -- --name <TestName>


