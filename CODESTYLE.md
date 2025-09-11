# CODESTYLE

Practical, test‑first C++20 style. Optimized for clarity and maintainability.

## Language and toolchain
- **C++20**. MSVC 2022, /W4, `/permissive-`, `cxx_std_20`.
- `#pragma once` in all headers.
- Build with CMake; tests with Catch2 v3.

## Naming
- **Namespaces**: `f1tm` for public API.
- **Types**: `PascalCase` (e.g., `SimServer`, `TrackCircle`).
- **Functions**: `snake_case` (e.g., `estimate_stint_time`).
- **Variables**: `snake_case`.
- **Constants**: `kCamelCase` (e.g., `kMaxCap`). Use `<numbers>` not ad‑hoc macros.

## Headers and includes
- Public headers live in `include/f1tm/`. Keep them minimal.
- Do not include rendering or platform headers in public API.
- Prefer forward declarations in headers; include definitions in `.cpp`.

## Errors and contracts
- Validate inputs at boundaries (public APIs). Clamp where sensible.
- Prefer value types and immutable snapshots across threads.
- Keep thread‑unsafe utilities confined to one thread; document it.

## Tests
- One test file per unit: `tests/test_<unit>.cpp`.
- Use `SECTION` for scenarios. Use `std::mt19937` with fixed seed.
- Keep tests deterministic and fast.

## Formatting
- Keep functions short and cohesive. Extract helpers early.
- Column limit ~100. Avoid long parameter lists; prefer small structs.
- Suggested `.clang-format` (drop in repo root):

```yaml
BasedOnStyle: Google
IndentWidth: 2
ColumnLimit: 100
DerivePointerAlignment: false
PointerAlignment: Left
SortIncludes: true
AllowShortFunctionsOnASingleLine: Empty
SpaceBeforeParens: ControlStatements
BreakBeforeBraces: Attach
```

## Warnings
- Treat warnings seriously. Consider `/WX` for CI once stable.
- Avoid implicit conversions; be explicit in casts and units.

## Concurrency
- Use `std::atomic` for shared flags and scalars.
- One writer, one reader for `SnapshotBuffer`. Do not share mutable state.
- Do not expose platform macros (like `PI`). Use `<numbers>` in headers.

## Commits and PRs
- TDD: fail, pass, refactor.
- Update docs for architectural changes (`docs/ARCHITECTURE.md`, ADRs).
- Keep diffs minimal and focused.
