# GD Benchmark

A first-pass in-game benchmark mode for Geometry Dash.

It uses **Demons In Between / GDDL** tier data, generates a pool of levels in a selected difficulty interval, runs a fixed-time normal-mode test, records best normal percentage and attempts, and stores a dated benchmark result.

## 0.1 MVP

- Main-menu **BENCHMARK** button.
- In-game configuration with +/- controls for tier range, number of levels and minutes per level.
- Random level selection from Demons In Between's loaded GDDL table.
- Optional "unseen only" filter (normal progress must be 0 when the level is already cached).
- Automatic level download polling through `GameLevelManager::downloadLevel`.
- Normal-mode benchmark timer; practice mode does not advance the timer.
- Automatic collection of normal best percentage and attempt delta.
- Saved benchmark history.

## Build

Install Geode SDK/CLI for Geometry Dash 2.2081, make sure Demons In Between is installed, set `GEODE_SDK`, then:

```text
geode build
```

The resulting `.geode` package can be installed locally from Geode.

## Important

This project has not been compiled against a local Geode SDK in the authoring environment, so it is a source-level first implementation rather than a runtime-verified binary. The two places most likely to need a tiny SDK-version adjustment are the `Popup<>` UI helper and the exact `GameLevelManager::getSavedLevel` binding.
