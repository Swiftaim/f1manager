# ADR-0004 Client interpolation
Status: Accepted
Date: 2025-09-11

## Context
Render FPS and server tick rate can diverge; we want smooth visuals without blocking the render thread.

## Decision
Client maintains an **InterpBuffer** keyed by `sim_time` and samples with **clamping** (no extrapolation). Use a small **interpolation delay** (~50 ms).

## Consequences
- Smooth motion even with frame jitter.
- Slight latency (tunable) in visuals versus authoritative sim state.
- Same shape will be needed when server moves out‑of‑process.
