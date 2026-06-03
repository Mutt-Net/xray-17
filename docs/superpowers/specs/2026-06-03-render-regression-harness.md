# Render-Regression Harness — Design (TEST-01 / TEST-02)

**Status:** SCAFFOLD. The design and the offline comparator are delivered and self-tested. The
end-to-end harness is **blocked** on prerequisites that do not exist in the build environment
(see §6). TEST-01/02 remain open in the triage until those are met.

## 1. Problem

Every render fix through release `2026.6.2` was validated by eye ("launch it and look"). There
is no automated way to tell whether a render change altered the image. This is the project's
single highest-leverage capability gap (future-development.md, Track B): without it, Track A and
any refactor are done with dread instead of confidence.

## 2. Approach — golden-image perceptual diff

1. **Fixture.** A small, committed set of camera poses on one deterministic level (a self-
   contained test map with fixed weather/time-of-day, no A-Life), expressed in
   `cameras.ltx` (position, direction, fov, the backend list, and per-shot settings).
2. **Capture.** A capture driver loads the level headless-ish (windowed, fixed resolution),
   teleports the camera to each pose, lets the frame settle N frames (deterministic — no
   random weather, A-Life paused), and writes a PNG per (camera, backend).
3. **Golden.** The first vetted capture per (camera, backend) is committed as the reference PNG
   under `tools/render_regression/golden/<backend>/<camera>.png`.
4. **Compare.** On each run, new captures are diffed against goldens with a **perceptual**
   metric (not exact equality — GPUs/drivers differ in the last ULP). A shot passes if its
   perceptual distance is below a per-shot tolerance. Output: a pass/fail report plus a diff
   heat-map PNG per failing shot.
5. **CI.** A workflow runs the capture on a GPU runner, invokes the comparator, and fails the
   job on any shot over tolerance, uploading the diff images as artifacts.

## 3. Perceptual metric

The comparator (`compare.py`) implements a self-contained metric with **no third-party deps**
so it runs anywhere Python 3 does:

- Decode both PNGs (stdlib `zlib` + a minimal PNG reader — RGBA8, no interlace).
- Per-pixel: compute a luma-weighted channel delta; accumulate (a) mean absolute error and
  (b) the fraction of pixels whose max-channel delta exceeds a "hot pixel" threshold.
- A shot **fails** if `mean_abs_error > tol_mae` OR `hot_pixel_fraction > tol_hot`.
- Defaults: `tol_mae = 2.0` (out of 255), `tol_hot = 0.002` (0.2% of pixels), `hot_threshold
  = 16`. These absorb driver/AA jitter while catching real regressions (a missing pass, a
  shifted half-pixel offset, a broken format). Per-shot overrides come from `cameras.ltx`.

The metric is intentionally simple and explainable; it can be upgraded to SSIM/Δometric later
without changing the harness contract (PNG in, pass/fail + heat-map out).

## 4. Determinism requirements

For goldens to be stable the captured scene must be deterministic:

- Fixed level with baked lighting; weather/time-of-day pinned via console (`set_weather`,
  fixed `g_game_difficulty` irrelevant).
- A-Life and dynamic NPCs disabled on the fixture map (no wandering geometry).
- Fixed resolution and a fixed graphics-preset per backend captured into the golden set.
- Disable film-grain / noise / temporal effects (or accept them via a looser per-shot `tol`).

## 5. Files (this scaffold)

| Path | Role | State |
|------|------|-------|
| `tools/render_regression/compare.py` | Perceptual-diff comparator + `--selftest` | Delivered, self-tested |
| `tools/render_regression/cameras.example.ltx` | Camera-fixture template | Delivered |
| `tools/render_regression/README.md` | Operator guide + blockers | Delivered |
| `.gitea/workflows/render-regression.yml` | CI workflow (manual/gated) | Delivered, inert until runner exists |
| `tools/render_regression/golden/` | Reference PNGs | **Missing** — needs a GPU + level |
| capture driver (engine console script) | Drives the engine to capture | Template only — needs the engine + assets |

## 6. Why it cannot be completed here (blockers)

- **No game install / gamedata.** The engine cannot load a level without the full Anomaly asset
  set (~10 GB), which is not in the repo and not on this worker.
- **No GPU.** This worker cannot render frames, so no goldens can be produced and no capture
  can run.
- **Dormant CI runner.** The Gitea runner has been offline since 2026-03-29 and the available
  token is not repo-owner, so CI cannot be exercised (blocked behind BUILD-01/02).

When BUILD-01/02 land a GPU runner and a level fixture is chosen, closing TEST-01/02 is:
capture goldens once, commit them, flip the workflow trigger from `workflow_dispatch` to
`push`, and add the capture-driver step.

## 7. Done criteria (for when unblocked)

- [ ] `golden/<backend>/<camera>.png` committed for all four backends on the fixture level.
- [ ] `compare.py` returns non-zero on a deliberately-broken render and zero on an unchanged one.
- [ ] CI runs the capture + compare on push and uploads diff artifacts on failure.
