# Testing strategy

- **Unit**: core math and models (stint, pit, race, track, events, sim)
- **Concurrency seam**: SnapshotBuffer behavior (publish/consume), InterpBuffer math
- **Executable tests**: Catch2 with `catch_discover_tests`

## How to run

```powershell
.\cpp test
.\cpp test --direct -- --success
.\cpp test --regex InterpBuffer
