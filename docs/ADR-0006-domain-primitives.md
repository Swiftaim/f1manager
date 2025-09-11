# ADR-0006 Domain primitives
Status: Accepted
Date: 2025-09-11

## Context
Strategy logic should be independent of rendering and simulation loop to remain testable.

## Decision
Model **stint**, **pit**, **race**, **track**, and **events** as pure functions/structs with dedicated unit tests.

## Consequences
- High testability; straightforward composition.
- UI and sim can evolve without changing domain contracts.
