# F1 Team Manager — Vertical Slice

A co-hosted client & server sim for a single-car top-down demo.  
**Tech**: C++20 • CMake • MSVC 2022 • Catch2 • raylib

## Quick Start

```powershell
# Configure (VS 2022 generator)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
# Build tests and app
cmake --build build --config Debug --target f1tm_tests
ctest --output-on-failure -C Debug -T test --test-dir build
cmake --build build --config Debug --target f1tm_app
# Run
./build/Debug/f1tm_app.exe
```

> If you’re using the included helper: `.\cpp configure`, `.\cpp build --target f1tm_app`, `.\cpp run --target f1tm_app`.

## Technical Documentation

- 📘 **F1Manager Technical Documentation** — systems design, class/struct relationships, and call paths:  
  [`docs/F1Manager-Technical-Documentation.md`](docs/F1Manager-Technical-Documentation.md)
- 🧭 **Architecture Overview** — components, threading, time model:  
  [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- 🧾 **ADR Index** — decision log:  
  [`docs/ADRs/INDEX.md`](docs/ADRs/INDEX.md)
- ✍️ **Code Style** — naming, headers, tests, formatting, `.clang-format`:  
  [`docs/CODESTYLE.md`](docs/CODESTYLE.md)

## Project Layout

```
include/f1tm/   # public headers (core API)
src/            # core implementations
apps/viewer/    # co-hosted client (render) + server (sim) in one process
tests/          # Catch2 tests
docs/           # architecture docs and ADRs
```

## CI (optional)

If you use GitHub Actions, add a badge (replace `<you>/<repo>`):

```markdown
![CMake (Windows MSVC)](https://github.com/<you>/<repo>/actions/workflows/cmake-msvc.yml/badge.svg)
```

---

Happy hacking! PR checklist lives at `docs/PR-CHECKLIST.md` if you adopt the suggested flow.
