# Backlog Schedule — all epics/tickets assigned to sprints

**Created:** 2026-06-03. Schedules every ticket in `docs/future-development.md` §6 into a sprint,
per owner request ("schedule all tasks on the backlog"). Ordering follows the dev plan's
"Suggested epic ordering" (foundation → correctness → perceived → throughput → strategic) and the
per-epic sequencing.

**Feasibility tags** (honest, set against *this* build environment — Windows + VS18, no game
assets, no GPU, no live game runtime):
- `[code]` — completable here: source change + build-verify, no runtime needed.
- `[runtime]` — needs the game running (assets + render device) to implement/verify.
- `[assets]` — needs the ~10 GB Anomaly gamedata.
- `[hw]` — needs specific hardware (GPU/VRR/HDR display, Steam Deck, ARM, etc.).
- `[XL]` — multi-month effort.
- `[infra]` — needs CI runners / external services.

A ticket can carry several tags. `[code]` tickets are the ones I can actually finish and ship to
`origin/dev`; the rest are scheduled but gated on the tagged dependency and will be handled
(implemented when the dependency exists, or mothballed by owner decision) when their sprint comes.

> NOTE on the goal loop: TEST-01/02 were owner-mothballed (2026-06-03). Several tickets below are
> `[runtime]`/`[assets]`/`[hw]`/`[XL]` and cannot be *completed* in this environment regardless of
> effort; they are scheduled (assigned + sequenced) as requested, not claimable as done here.

---

## Sprint s4 — Track A finish + Track C (correctness)  ← IN PROGRESS

| Ticket | Tag | Note |
|--------|-----|------|
| RND-04 | `[code]` | Document/resolve `nullrt` R5G6B5→R8G8B8A8 workaround |
| RND-05 | `[code]` | Shader-resource HACK (dx10ResourceManager_Resources.cpp:177,289,389) |
| RND-06 | `[code]` | Remove DX9 state-cache leftovers (dx10StateCache.cpp) |
| RND-07 | `[code]` | Triangle-fan topology handling |
| RND-08 | `[code]` | Commented scripting paths (dx10ResourceManager_Scripting.cpp) |
| RND-09 | `[code]` | Empty/obsolete TODO + `VERIFY(!"Implement shader object parsing")` (dx10r_constants.cpp:150) |
| RND-10 | `[code]` | Sweep remaining TODO/HACK in render dirs; classify each |
| RND-11 | `[code]` | R3/R4 parity check note/guard |
| CODE-04 | `[code]` | ✓ **Substantially DONE 2026-06-05** — audited all render crash-stubs (`docs/code-04-crash-stub-audit.md`); no-op'd the debug ones (overdrawBegin/End, dbg_SetRS/SS); render/gameplay stubs (pick_bone, smap tsh, CHW::support, RND-09) left asserting + catalogued for `[runtime]` |
| CODE-03 | `[code]` | Engine-wide dead/commented-code sweep (the `/* DX10 cut */` pattern) |
| Track C | `[runtime]` | Debug-layer-clean DX11: the *audit* of warnings needs a running game; code fixes land as found |

## Sprint s5 — Code health + build/infra

| Ticket | Tag | Note |
|--------|-----|------|
| CODE-01 | `[code]` | Raise warning level, clear warnings, then `-Werror` |
| CODE-05 | `[code]` | const-correctness pass |
| CODE-08 | `[code]` | Smart-pointer adoption in new code |
| CODE-02 | `[code]` | Selective C++20 adoption |
| CODE-06 | `[infra]` | Enable IWYU |
| CODE-07 | `[infra]` | clang-format baseline |
| MEM-01 | `[code]` | ✓ **DONE 2026-06-03** — exes are x64 + `/LARGEADDRESSAWARE` (verified from link flags) |
| BUILD-03 | `[infra]` | Re-enable MSVC presets in release matrix |
| BUILD-04 | `[infra]` | ccache/sccache in CI |
| BUILD-07 | `[code]` | ✓ **DONE 2026-06-04** — `docs/submodules.md` records pinned SHAs + anti-drift policy; fixed a duplicate `Externals/optick-git` stanza in `.gitmodules` |
| BUILD-09 | `[code]` | dev integration branch + branch protection (**dev branch DONE 2026-06-03**) |
| BUILD-10 | `[infra]` | Automated changelog from commits |
| BUILD-12 | `[code]` | ✗ **WON'T FIX 2026-06-04** — the `vswhere` logic is in the vendored `Externals/WindowsToolchain` submodule (`VSWhere.cmake`); the message is benign (build succeeds, vswhere is found) and patching a submodule would diverge it. Upstream-only fix. |
| BUILD-01/02 | `[infra]` | Register runners (**Windows runner DONE**; Linux pending) |
| BUILD-05/06/08/11 | `[infra]` | PCH/unity audit, build-time profiling, reproducible builds, PDB archival |

## Sprint s6 — Stability/diagnostics + documentation

| Ticket | Tag | Note |
|--------|-----|------|
| CRASH-01 | `[code]` | Modern crash handler + minidumps |
| CRASH-04 | `[code]` | Friendly in-game error reporting |
| CRASH-05 | `[code]` | Hang watchdog |
| CRASH-02 | `[infra]` | Symbol server for release PDBs |
| CRASH-03 | `[infra]` | Opt-in crash telemetry |
| DOCS-01..05 | `[code]` | ✓ **DOCS-01 DONE** (`docs/architecture.md`), ✓ **DOCS-02 DONE** (`docs/building.md`), ✓ **DOCS-05 DONE** (`docs/render-r3-r4-mirror.md`) — 2026-06-03/04. Remaining: DOCS-03 (per-subsystem READMEs), DOCS-04 (modder API ref). |
| SEC-01 | `[code]` | Audit crypto module |
| SEC-02..04 | `[code]`/`[runtime]` | Harden asset/config parsing, save integrity, net protocol |

## Sprint s7 — Performance (profile-first)

| Ticket | Tag | Note |
|--------|-----|------|
| PERF-01 | `[runtime]` | Profiling pass — measure before optimising |
| PERF-02 | `[code]` | Share input signatures |
| PERF-03/04 | `[code]` | Geometry-buffer fragmentation audit; volumetric light sorting |
| PERF-08..14 | `[runtime]`/`[code]` | AVX benchmark, PSO cache, sync-point audit, pacing, streaming, async load |
| PERF-05 | `[XL]` | Multithreaded render submission |
| PERF-06/07 | `[code]`/`[runtime]` | Batching/instancing; GPU occlusion culling |

## Sprint s8 — Display / input / audio

DISP-01..08 `[hw]`/`[code]` · INPUT-01..04 `[code]`/`[runtime]` · AUDIO-01..04 `[code]`/`[runtime]`

## Sprint s9 — Graphics features

GFX-02 (FSR) `[runtime]` · GFX-04 (TAA) `[runtime]` · GFX-05 (HDR refine) `[hw]` ·
GFX-08/09/10/11/12 `[runtime]` · GFX-07 `[runtime]`

## Sprint s10 — Platform / localisation / config / distribution

PLAT-01..04 `[runtime]`/`[hw]` · I18N-01..03 `[code]`/`[runtime]` · CFG-01..04 `[code]`/`[runtime]` ·
DIST-01..04 `[infra]`

## Backlog tail — strategic / XL (scheduled "later", explicitly long-horizon)

GFX-01 (DX12/Vulkan R5) `[XL]` · GFX-13 (RT) `[XL]` · GFX-03/06 `[hw]` · PERF-05 `[XL]` ·
AI-01..07 `[runtime]`/`[XL]` · PHYS-01..04 `[runtime]`/`[XL]` · ENV-01..05 `[runtime]`/`[XL]` ·
SUBSYS-01..05 `[runtime]` · NET-01..03 `[runtime]` · MOD-01..06 `[runtime]` · A11Y-01..05 `[runtime]` ·
REPLAY-01..03 `[XL]`/`[runtime]` · MEM-02..05 `[runtime]` · PLAT-02 `[hw]`

## Mothballed (owner decision, not scheduled)

TEST-01, TEST-02 — render-regression golden capture. Parked under `tools/render_regression/`.

---

**Execution rule:** work `[code]` tickets to completion + build-verify + ship to `origin/dev`,
sprint by sprint. `[runtime]`/`[assets]`/`[hw]`/`[XL]`/`[infra]` tickets are scheduled here but
cannot be completed in this environment — they await their dependency (a game-capable runner,
specific hardware, CI runners) or an owner call, exactly as TEST-01/02 did.
