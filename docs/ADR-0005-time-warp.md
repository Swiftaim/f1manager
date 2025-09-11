# ADR-0005 Time warp
Status: Accepted
Date: 2025-09-11

## Context
For debugging and demos we need pause and speed scaling, without compromising determinism.

## Decision
Expose an atomic **time_scale** controlling the effective dt: `dt_eff = base_dt * time_scale`. Publish snapshots even when paused.

## Consequences
- Deterministic and thread‑safe control surface.
- For warp > 1x, consider **sub‑stepping** to preserve integrator stability (future ADR).
