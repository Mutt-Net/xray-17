# Render-Regression Harness (TEST-01 / TEST-02)

A golden-image perceptual-diff safety net for the four renderer backends. **Status: MOTHBALLED
(de-scoped 2026-06-03).** This directory is parked for future revival, not active work. The
comparator works and is self-tested, but the end-to-end harness was de-scoped (needs gamedata
assets + a render-capable runner the project doesn't maintain). The CI workflow was moved to
`render-regression.yml.parked` so it is de-registered from CI. Design:
`docs/superpowers/specs/2026-06-03-render-regression-harness.md`.

## What's here

| File | Purpose |
|------|---------|
| `compare.py` | Self-contained (stdlib-only) perceptual PNG comparator. `--selftest` validates it with no GPU. |
| `cameras.example.ltx` | Camera-fixture template (poses, resolution, per-shot tolerances). |
| `.gitea/workflows/render-regression.yml` | CI workflow — **manual/gated** until a GPU runner exists. |
| `golden/<backend>/<camera>.png` | Reference images. **Not yet captured** (needs GPU + level). |

## Comparator usage

```bash
# Verify the comparator itself (no GPU, no assets needed):
python tools/render_regression/compare.py --selftest

# Compare one capture against its golden, writing a diff heat-map on failure:
python tools/render_regression/compare.py \
    --golden  golden/DX11/shot_overview.png \
    --candidate captured/DX11/shot_overview.png \
    --diff    diffs/DX11/shot_overview.png

# Compare a whole captured tree against goldens (this is what CI runs):
python tools/render_regression/compare.py \
    --golden-dir tools/render_regression/golden \
    --candidate-dir captured \
    --diff-dir diffs
```

Exit code is `0` only if every shot passes; non-zero (with `[FAIL]`/`[MISS]` lines) otherwise —
that is the CI gate. Pass criteria and tolerances are documented at the top of `compare.py`.

## Bringing it up (when unblocked)

1. Pick/author a deterministic fixture level (`rr_testmap`): baked lighting, A-Life off, weather
   and time-of-day pinned. Copy `cameras.example.ltx` → `cameras.ltx` and set real poses.
2. Implement the capture driver (engine-side console script / `-r2_capture` style switch) that
   reads `cameras.ltx`, teleports the camera, settles `settle_frames`, and screenshots to
   `captured/<backend>/<shot>.png`. The existing async-screenshot path is the hook point.
3. Capture once per backend, eyeball the results, commit them as `golden/<backend>/<shot>.png`.
4. In the workflow, add the capture step and flip the trigger from `workflow_dispatch` to `push`.

## Blockers (why it isn't finished)

- **No game install / gamedata** on the build worker — the engine can't load a level.
- **No GPU** available to the worker — can't render, so can't produce goldens.
- **CI runner dormant** since 2026-03-29; closing this is gated behind BUILD-01/BUILD-02
  (register Windows + Linux runners) and a chosen fixture level.
