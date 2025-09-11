# ADR-0003 Snapshot pipe
Status: Accepted
Date: 2025-09-11

## Context
The server (sim) and client (render) run on separate threads and must exchange state safely with minimal latency.

## Decision
Implement a **single‑producer single‑consumer** latest‑only **SnapshotBuffer**.

## Consequences
- Very low contention and simple semantics (latest wins).
- No historical queueing; the client is responsible for keeping history (Interpolation buffer).
- Easy to swap with a network/IPC publisher later.
