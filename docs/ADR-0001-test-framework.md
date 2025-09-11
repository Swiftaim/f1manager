# ADR-0001 Test framework
Status: Accepted
Date: 2025-09-11

## Context
We need a lightweight, ergonomic C++ test framework to support TDD with minimal ceremony.

## Decision
Use **Catch2 v3** via CMake **FetchContent** and `Catch2::Catch2WithMain`.

## Consequences
- Simple test macros (`TEST_CASE`, `SECTION`, `REQUIRE`).
- Deterministic floating comparisons via `catch_approx.hpp`.
- Fast red→green loops; no external installations needed.
