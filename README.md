# GD Benchmark v0.1.0

First prototype of an in-game skill benchmark for Geometry Dash.

## What this version does

- uses Demons In Between / GDDL data for demon tiers;
- lets you choose a tier range, 4–6 levels, test duration, and an unseen filter;
- picks a random test set;
- downloads missing levels through Geometry Dash;
- starts the selected levels in normal mode;
- tracks active benchmark time, best normal %, attempts, and practice-mode usage;
- stores raw per-level observations and benchmark history in Geode saved data;
- shows a simple history window.

The rating formula is deliberately only an MVP placeholder based on the original HTML calculator. The raw observations are retained so the statistical model can be improved later without redoing old tests.

## Easiest way to build without installing a C++ toolchain locally

This repository includes `.github/workflows/build.yml`.

The official Geode build action sets up the Geode CLI/SDK and builds a Windows `.geode` package on a GitHub Actions runner. The resulting `.geode` is uploaded as a workflow artifact.

1. Put this project into a GitHub repository.
2. Open the repository's **Actions** tab.
3. Open **Build GD Benchmark**.
4. Choose **Run workflow** (or simply push a commit; the workflow also runs on pushes).
5. When the workflow finishes successfully, open the run and download the artifact **GD-Benchmark-Windows**.
6. Inside it will be the `.geode` file you can install with Geode.

No CMake, compiler, Geode CLI, or Geode SDK needs to be installed on your own PC for this route.

## Local build

If you later decide to build locally, current Geode documentation recommends the Geode CLI and `geode build`.

## Dependency

This mod depends on Demons In Between (`hiimjustin000.demons_in_between`) and uses its public `DemonsInBetweenLoadedEvent` API to receive the `ID -> LadderDemon` table.
