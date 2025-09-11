# ADR-0002 Graphics library
Status: Accepted
Date: 2025-09-11

## Context
We need a tiny 2D rendering layer for a top‑down vertical slice and fast iteration.

## Decision
Use **raylib v5** via CMake **FetchContent**.

## Consequences
- Small, portable API; easy drawings (lines, circles, text).
- Beware of global macros (e.g., `PI`). In our headers, use `<numbers>` instead.
- Future: renderer remains replaceable; sim code is UI‑agnostic.
