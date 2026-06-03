# Sprint s3 — Design / Spec

**Date:** 2026-06-03
**Branch:** `sprint/s3`
**Tracks:** Track A start (Epic RND) + Track B scaffold (Epic TEST)
**Scheduled scope (per `docs/future-development.md` §4 "Concrete next-step candidates for sprint s3"):**

1. Complete the `CRTC` (cube render-target) story in the resource manager — **RND-01**, **RND-02**.
2. Resolve the `set_xform` stub — **RND-03**.
3. Stand up a golden-image render-regression harness for one level, all four backends, in CI — **TEST-01**, **TEST-02**.

---

## 1. Pre-implementation investigation (what is actually true)

Sprint plans for s1/s2 took the triage notes at face value. Before writing code for s3 the
render tree was re-examined against the build graph. Two of the three "CRTC" framings in the
backlog turned out to be **inaccurate**, and the spec is corrected here so the implementation
matches reality rather than the ticket prose.

### 1.1 The CRTC apparatus is vestigial across every backend

A search for *callers* (not definitions) of `_CreateRTC` / `resptrcode_crtc::create` / `ref_rtc`
across all of `src/` returns **only definitions** — there is no call site anywhere in the
engine or game that constructs a cube render target. `CRTC` is leftover infrastructure from
old X-Ray cube-shadow paths. s2 ported the DX10/11 implementation for correctness/symmetry,
but it is dormant code.

### 1.2 The DX9 base files cannot host `_CreateRTC` — it is `#if`-guarded out there

`CRTC`, `map_RTC`, `m_rtargets_c`, and `_CreateRTC`/`_DeleteRTC` are all wrapped in
`#if defined(USE_DX10) || defined(USE_DX11)` in the shared headers:

- `SH_RT.h` lines 53–81 (the `CRTC` class + `resptrcode_crtc` + `ref_rtc`).
- `ResourceManager.h` lines 41–43 (`map_RTC`), 63–65 (`m_rtargets_c`), 155–157 (`_CreateRTC`/`_DeleteRTC`).

Build-graph facts (from the `XRay.Render.*` cmake modules):

| File | Compiled into | `m_rtargets_c` / `CRTC` visible? |
|------|---------------|----------------------------------|
| `xrRender/ResourceManager_Resources.cpp` (`/* DX10 cut */` `_CreateRTC` at :372) | **R2/DX9 only** | **No** — guarded out |
| `xrRender/SH_RT.cpp` (`/* DX10 cut */` `CRTC` at :120) | **R2/DX9 only** | **No** — guarded out |
| `xrRenderDX10/dx10ResourceManager_Resources.cpp` (live `_CreateRTC` at :528) | **R3/R4 only** | Yes |
| `xrRenderDX10/dx10SH_RT.cpp` (live `CRTC` at :228) | **R3/R4 only** | Yes |
| `xrRender/ResourceManager_Reset.cpp` (`/* DX10 cut */` dump at :128) | **R1/R2/R3/R4 (shared)** | Only under DX10/DX11 |

Therefore:

- **RND-01 as literally written ("uncomment/complete `_CreateRTC` in the shared `xrRender`
  resource manager") is not actionable and would be wrong.** That file is DX9-only; the symbols
  it references do not exist in the DX9 build. The real `_CreateRTC`/`CRTC` already exist and
  compile for DX10/11 (done in s2). The cube-RT path is *not* "half-wired" — it is fully wired
  for DX10/11 and intentionally absent for DX9.
- The correct RND-01 action is **cleanup**: delete the dead, misleading `/* DX10 cut */` CRTC
  blocks from the two DX9-only files and leave a one-line pointer to the DX10/11 home of the
  feature, so a future reader is not lured into "finishing" a port that is already finished
  elsewhere and would not compile here.

### 1.3 `ResourceManager_Reset.cpp:128` is the one genuinely-shared cut — RND-02 is real

`Dump()` lives in the shared, all-backend `ResourceManager_Reset.cpp`. The cut line is the
`rtargetsc` branch of the resource census. Restoring it under
`#if defined(USE_DX10) || defined(USE_DX11)` makes the DX10/11 dump symmetric with `m_rtargets`
without referencing a member that the DX9 build lacks. Low risk, P3, genuinely actionable.

### 1.4 `set_xform(u32 ID, const Fmatrix&)` is dead on the runtime — RND-03

- Declared in shared `R_Backend.h:257`.
- DX10/11 definition: `dx10R_Backend_Runtime.h:8` — increments `stat.xforms` then a
  commented-out `//VERIFY(!"Implement CBackend::set_xform");`.
- Only caller anywhere: `R_Backend_Runtime.h:167` inside `set_Matrices`, which is wrapped in
  `#ifdef _EDITOR`. The shipping `AnomalyDX*.exe` runtime is **not** built with `_EDITOR`.
- `set_xform_world/view/project` (the live ones) do **not** route through it — they call
  `xforms.set_W/V/P` directly.

Conclusion: **confirmed dead** on the DX10/11 runtime, and effectively dead everywhere (the
sole caller is editor-only). Deleting the declaration outright is a cross-backend change (the
DX9 build still defines it for its editor path) for zero runtime benefit and non-zero build
risk. The proportionate resolution that removes the "unfinished stub" ambiguity is to replace
the commented-out `VERIFY` with a **knowledge comment** stating it is a deliberate no-op and
why. This is the same disposition s1 used for confirmed-dead Group H items, and it satisfies
RND-03's "no commented `VERIFY` middle ground".

---

## 2. Track A change set (Epic RND)

| Ticket | File(s) | Change |
|--------|---------|--------|
| RND-01 | `xrRender/ResourceManager_Resources.cpp`, `xrRender/SH_RT.cpp` | Remove the dead `/* DX10 cut */` `_CreateRTC`/`_DeleteRTC` and `CRTC` blocks; add a one-line note pointing to `xrRenderDX10/` as the DX10/11 home. No behavioural change (these blocks were never compiled). |
| RND-02 | `xrRender/ResourceManager_Reset.cpp` | Replace the commented `// DX10 cut` dump line with a real `rtargetsc` census, guarded `#if defined(USE_DX10) || defined(USE_DX11)`. |
| RND-03 | `xrRenderDX10/dx10R_Backend_Runtime.h` | Replace the commented `VERIFY` with a knowledge comment (deliberate no-op: no fixed-function transform stack on DX10+, sole caller is `_EDITOR`-only). |

**Invariant — R3/R4 parity (RND-11):** none of the RND-01/02/03 edits touch R3-vs-R4 mirror
code, so the pair stays in sync by construction. RND-02 and RND-03 live in `xrRender/` and
`xrRenderDX10/` files that are shared by both R3 and R4.

## 3. Track B scaffold (Epic TEST)

TEST-01 (golden-image harness) and TEST-02 (CI wiring) are **P1** and the highest-leverage
capability gap. They cannot be *completed* in this environment:

- No S.T.A.L.K.E.R. Anomaly game install / `gamedata` is present in the repo (the engine is
  useless without ~10 GB of assets to load a level).
- No GPU is available to this worker to actually render frames.
- The Gitea CI runner has been dormant since 2026-03-29 and the Hol token is not repo-owner,
  so CI cannot be exercised.

What s3 *can* deliver honestly is the **design + scaffold** so the harness is ready to run the
moment a runner with a GPU and a level fixture exists:

- `docs/superpowers/specs/2026-06-03-render-regression-harness.md` — the harness design
  (capture protocol, camera fixture format, perceptual-diff metric + tolerance, golden storage,
  CI integration point).
- `tools/render_regression/` — the comparison tool (`compare.py`, perceptual diff with a
  documented tolerance) and a capture-driver console script template, runnable offline against
  pre-captured PNGs. The comparator is unit-testable without a GPU and is verified here.
- `.gitea/workflows/render-regression.yml` — a CI workflow, **gated/manual** (`workflow_dispatch`
  + guarded `if`) so it does not fail the dormant pipeline, wired to invoke the comparator.

These are marked clearly as scaffold; TEST-01/02 remain **open** in the triage until a runner
and a level fixture close them. Claiming them "done" would be false.

## 4. Verification

- Build all four targets with the warm MSVC/Ninja preset:
  `cmake --build --preset MSVS.MSVC.Ninja.RelWithDebInfo`. Expect a clean incremental build
  (the RND edits are tiny; the only compiled change is RND-02 + RND-03).
- Run the comparator's offline self-test (`python tools/render_regression/compare.py --selftest`)
  to verify the perceptual-diff logic without a GPU.
- `grep` the touched files to confirm the `/* DX10 cut */` and commented-`VERIFY` markers are
  gone.

## 5. Out of scope (explicitly deferred to s4+, per §4 sequencing)

RND-04..RND-10 (remaining Group H/I knowledge-comment cleanups), Track C (debug-layer clean),
Epic TEST-03+ (unit/smoke tests), and everything in the P3 "menu". s3 is deliberately the three
scheduled RND tickets + the Track B scaffold, nothing more.
