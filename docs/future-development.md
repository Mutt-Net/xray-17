# Future Development — X-Ray Monolith Edition

A strategic assessment of the codebase as it stands after releases through `2026.6.2`,
and a prioritised set of development tracks. Written to inform sprint planning and to
give external contributors a map of where effort pays off.

---

## 1. Where the project stands

This is a mature community fork of the X-Ray engine targeting S.T.A.L.K.E.R. Anomaly
1.5.3. It builds four renderer backends (R1/DX8, R2/DX9, R3/DX10, R4/DX11) from one
codebase, with a recently-modernised CMake build system, a CI pipeline that produces
dated releases, and an enforced code-style gate.

The engine is **playable and shipping**. The work in recent sprints has been correctness
and stability, not features. The honest one-line assessment: *the renderer works, but the
DX10/DX11 paths still carry the scar tissue of an incomplete port, and there is no
automated safety net to refactor against.*

Two structural facts shape everything below:

1. **R3 and R4 are near-mirrors of R2.** The DX10/DX11 renderers were produced by porting
   the DX9 renderer file-by-file. Shared logic lives in `xrRenderDX10/` behind the
   `DXCommonTypes.h` alias layer (`ID3D*`, `D3D_*` macros switch between D3D10 and D3D11).
   This is why most fixes land in pairs (R3 + R4) and why the alias layer is the highest-
   leverage file in the render tree.

2. **The port was never finished.** Across the render layer there remain `/* DX10 cut */`
   blocks (commented-out DX9 code never re-implemented), `VERIFY(!"Not implemented.")`
   stubs, and a tail of `// TODO: DX10:` markers. Recent sprints cleared the worst of
   these, but the pattern is the dominant source of latent bugs.

---

## 2. The technical-debt spine

Everything traces back to the incomplete port. Concrete, observed remnants as of
`2026.6.2`:

| Item | Location | Class | Notes |
|------|----------|-------|-------|
| Cube RT base path still cut | `xrRender/ResourceManager_Resources.cpp:372`, `ResourceManager_Reset.cpp:128` | missing-feature | Sprint s2 ported the DX10/11 `CRTC`; the **shared** resource manager (`_CreateRTC`/`_DeleteRTC`) and reset-time dump are still `/* DX10 cut */`. Cube RT support is therefore half-wired. |
| `set_xform` stub | `xrRenderDX10/dx10R_Backend_Runtime.h:8` | stub | Commented `VERIFY(!"Implement CBackend::set_xform")` — effectively a no-op on DX10/11. Needs an audit of whether any call path depends on it. |
| `nullrt` format workaround | `xrRenderDX10/dx10TextureUtils.cpp:17` | hack | R5G6B5→R8G8B8A8 because B5G6R5 is unavailable; documented but not resolved. |
| Group H open items | render-todo-triage | mixed | Shader-resource HACK in `dx10ResourceManager_Resources.cpp`, commented scripting paths, DX9 state-cache leftovers, triangle-fan topology unsupported. |
| Residual TODO/HACK | ~9 hits in `xrRenderDX10/`, plus R3/R4 | mixed | Mostly P3 knowledge comments now, but a few are genuine gaps. |

**Risk character:** these are not crashes (the crash sites were the priority and are
fixed). They are *silent* failures — an effect that quietly does nothing, a format that
falls back, a resource that never gets created. They surface as visual bugs that are hard
to attribute without a render-debugging session.

---

## 3. Development tracks

Seven tracks, each with an honest effort/value/risk read. Tracks A and B are the
recommended near-term focus.

### Track A — Finish the DX10/DX11 port (correctness)
**Effort:** M · **Value:** High · **Risk:** Low

The natural continuation of sprints s1/s2. Systematically eliminate every remaining
`/* DX10 cut */` block, stub, and genuine TODO in the render layer. Each is independently
shippable and cherry-pickable — ideal incremental work.

- Complete the shared `CRTC` path (base resource manager + reset dump) so cube RTs are
  end-to-end functional, not half-wired.
- Resolve `set_xform`: confirm whether it is dead or load-bearing, then either implement
  or delete it outright (no commented `VERIFY` middle ground).
- Triage the remaining Group H shader-resource HACK and state-cache leftovers.

**Why first:** lowest risk, highest correctness return, and it shrinks the surface area
that every other track has to reason about.

### Track B — Render-regression test harness
**Effort:** M · **Value:** High · **Risk:** Low

The project's biggest *capability* gap is the absence of an automated visual safety net.
Verification today is "launch it and look." Every render fix in this release was validated
by eye.

Propose a golden-image harness: a headless or scripted level load, capture a fixed set of
camera positions per DX backend, diff against committed reference PNGs with a perceptual
tolerance. Wire it into CI alongside the existing format checks.

**Why early:** it is a force-multiplier. With it, Track A (and any future refactor) can be
done with confidence instead of dread. Without it, the codebase resists change.

### Track C — Debug-layer-clean DX11
**Effort:** S–M · **Value:** Medium · **Risk:** Low

Sprint s1 removed one per-frame D3D debug-layer error (the MSAA accumulator). Make
"debug layer is silent for a normal play session" an explicit quality bar. Run with
`--dxgi-dbg` + RenderDoc API validation, enumerate every warning/error, fix or
consciously suppress each. This is how the *next* class of silent bugs gets found.

### Track D — Performance: AVX, threading, profiling
**Effort:** M–L · **Value:** Medium · **Risk:** Medium

The AVX configure presets exist and the CI release builds AVX variants, but there is no
evidence of profiling-guided optimisation. X-Ray is historically CPU-bound on the render
submission and AI threads.

- Profile a representative heavy scene; find the actual hot paths before optimising.
- The render-todo triage already flags candidates (input-signature sharing to cut
  `InputLayout` churn, geometry-buffer fragmentation, light sorting).
- Treat AVX as a measured win, not an assumed one — verify it actually moves frame time.

### Track E — Linux-native / Proton hardening
**Effort:** L · **Value:** Medium (audience-dependent) · **Risk:** Medium

The presence of `System.GNU` presets and the DXGI "Linux fullscreen workaround" in the
community base signals latent interest. Anomaly is widely played via Proton. Options range
from *hardening the DX11 path under Proton/DXVK* (lower effort) to *a native Linux build*
(high effort, blocked by Windows-specific code throughout `xrCore`/`xrEngine`).

### Track F — Modern API backend (DX12 / Vulkan)
**Effort:** XL · **Value:** Strategic · **Risk:** High

A true R5 backend. This is the long-horizon modernisation play and should not be started
until Track B exists (you cannot safely build a new backend with no regression tests) and
Track A is done (you don't want to port half-finished code again). Realistically a
multi-month effort; list it to acknowledge it, not to schedule it soon.

### Track G — Upstream contribution
**Effort:** S · **Value:** Medium · **Risk:** Low

The sprint s1/s2 fixes are genuine correctness improvements with clean, documented
commits. Opening a PR to the upstream community repo (`Lander-Modding/xray-monolith`)
benefits the wider Anomaly community and reduces this fork's long-term merge burden.
`docs/muttcode-changes.md` is already written in a portable, reviewer-friendly form.

---

## 4. Recommended sequencing

```
Sprint s3  ──►  Track B (regression harness)  +  Track A start (cube RT completion)
Sprint s4  ──►  Track A finish (stubs, Group H)  +  Track C (debug-layer-clean DX11)
Sprint s5  ──►  Track G (upstream PR)  +  Track D (profiling pass, measured AVX)
Backlog    ──►  Track E (Linux/Proton),  Track F (modern backend)
```

Rationale: **build the safety net (B) first**, use it to **finish the port (A)** and
**clean the debug layer (C)**, then **give the work back (G)** and **chase measured
performance (D)**. The big strategic bets (E, F) wait until the foundation is solid.

### Concrete next-step candidates for sprint s3

1. Complete `CRTC` in the shared `xrRender` resource manager + reset path (closes the
   half-finished sprint-s2 work).
2. Stand up a minimal golden-image harness for one level, all four backends, in CI.
3. Resolve `set_xform` — implement or delete, no commented stub.

---

## 5. Process & infrastructure notes

- **CI release matrix** currently builds Clang + Clang.AVX only; MSVC presets are
  commented out in `anomaly-release.yml`. Local builds are verified on MSVC. Consider
  re-enabling MSVC in CI so the shipped toolchain and the developer toolchain match.
- **Branch model:** `cmake` is the active line; the release workflow triggers on push to
  it. A `dev` integration branch and branch protection on the release line would align
  with the documented `feature/* → dev → main` flow and prevent accidental releases.
- **Documentation:** `docs/muttcode-changes.md` (patch notes), `docs/render-todo-triage.md`
  (debt inventory), and per-release notes under `docs/releases/` now form a coherent set.
  Keep them current as the canonical record — they are what makes this fork adoptable.

---

## 6. Engine improvement backlog (tickets)

A full, deliberately exhaustive inventory of improvement ideas, organised by epic and
written as tickets. Not a commitment — a menu. Each ticket: **Class** (bug / tech-debt /
feature / perf / infra / docs), **Effort** (S < 1 day · M days · L weeks · XL months),
**Priority** (P1 ship-blocker-adjacent · P2 real value · P3 nice-to-have). IDs are stable;
add new ones by appending, never renumber.

Cross-reference: items already framed as Tracks A–G in §3 are tagged `[Track X]`.

### Epic RND — Finish the DX9→DX10/11 port (correctness) `[Track A]`

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| RND-01 | Complete `CRTC` in shared `xrRender` resource manager | tech-debt | S | P2 | `_CreateRTC`/`_DeleteRTC` still `/* DX10 cut */` at `ResourceManager_Resources.cpp:372`. Cube RTs only half-wired after sprint s2. |
| RND-02 | Restore cube-RT dump in reset path | tech-debt | S | P3 | `ResourceManager_Reset.cpp:128` cut block — `m_rtargets_c` not dumped on reset. |
| RND-03 | Resolve `set_xform` stub | tech-debt | S | P2 | `dx10R_Backend_Runtime.h:8` has a commented `VERIFY`; confirm dead vs load-bearing, then implement or delete. |
| RND-04 | Resolve `nullrt` R5G6B5→R8G8B8A8 workaround | tech-debt | S | P3 | `dx10TextureUtils.cpp:17`; B5G6R5 unavailable — document permanently or find a true equivalent. |
| RND-05 | Implement/clean shader-resource HACK | tech-debt | M | P2 | Group H — `dx10ResourceManager_Resources.cpp` "all shaders must be implemented". |
| RND-06 | Remove DX9 state-cache leftovers | tech-debt | S | P3 | `dx10StateCache.cpp:55,68,81`. |
| RND-07 | Triangle-fan topology handling | tech-debt | M | P3 | Early-return today; decompose to triangle lists or audit/kill DX9 callers. |
| RND-08 | Commented scripting paths | tech-debt | S | P3 | `dx10ResourceManager_Scripting.cpp:49,369`. |
| RND-09 | Empty TODO marker | tech-debt | S | P3 | `dx10r_constants.cpp:150`. |
| RND-10 | Sweep remaining `TODO/HACK` in render dirs | tech-debt | M | P3 | ~9 in xrRenderDX10 + R3/R4; classify each as fix vs knowledge comment. |
| RND-11 | Verify R3/R4 parity after every fix | tech-debt | S | P2 | R3 and R4 are mirrors; add a checklist/lint that flags divergence between the pair. |

### Epic TEST — Test & QA infrastructure `[Track B]`

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| TEST-01 | Golden-image render-regression harness | infra | M | P1 | Scripted level load, fixed cameras, per-backend capture, perceptual diff vs committed PNGs. The single highest-leverage capability gap. |
| TEST-02 | Wire regression harness into CI | infra | S | P1 | Run TEST-01 alongside the format checks; fail on perceptual delta. |
| TEST-03 | Unit tests for `xrCore` math | test | M | P2 | Matrix/quaternion/vector — pure, deterministic, high-value first test target. |
| TEST-04 | clang-tidy static analysis in CI | infra | M | P2 | Start with a curated rule set; ratchet over time. |
| TEST-05 | ASan/UBSan build configuration | infra | M | P2 | Catch memory/UB bugs the X-Ray codebase is prone to. |
| TEST-06 | Smoke test per backend | test | M | P2 | Launch → load level → tick N frames → clean exit, all four DX exes. |
| TEST-07 | Save/load round-trip tests | test | M | P3 | Serialise → deserialise → compare; guards the save format. |
| TEST-08 | Shader compilation validation in CI | infra | M | P2 | Compile every shader for every backend; fail on error/warning. |
| TEST-09 | Crash-repro corpus | test | M | P3 | Capture fixed inputs that previously crashed; keep them green. |
| TEST-10 | Determinism harness | test | L | P3 | Seeded run produces identical state; underpins replay/netcode/physics work. |

### Epic PERF — Performance `[Track D]`

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| PERF-01 | Profiling pass on a heavy scene | perf | M | P1 | Measure before optimising; find the real hot paths (render submit, AI, physics). |
| PERF-02 | Share input signatures | perf | M | P2 | Triage-flagged; cut `InputLayout` creation churn across VS variants. |
| PERF-03 | Geometry-buffer fragmentation audit | perf | M | P3 | `r3/r4_loader.cpp` flagged in triage. |
| PERF-04 | Volumetric light sorting | perf | S | P3 | `light_vis.cpp` flagged; sort to reduce overdraw/state changes. |
| PERF-05 | Multithreaded render command submission | perf | XL | P2 | Deferred contexts / command lists; the biggest CPU win on DX11. |
| PERF-06 | Draw-call batching / instancing | perf | L | P2 | Reduce per-object overhead in dense scenes. |
| PERF-07 | GPU occlusion-culling improvements | perf | L | P3 | Reduce wasted draws behind geometry. |
| PERF-08 | Benchmark AVX; add AVX2 path | perf | M | P3 | Verify AVX actually moves frame time before promoting it; consider AVX2. |
| PERF-09 | Pipeline-state / shader cache | perf | M | P2 | Cache compiled PSOs to cut hitching on first encounter. |
| PERF-10 | Reduce CPU↔GPU sync points | perf | M | P2 | Audit `Map`/`CopyResource`/query stalls (the screenshot path was one). |
| PERF-11 | Frame-time variance / pacing | perf | M | P2 | Smooth frame delivery; addresses microstutter independent of average FPS. |
| PERF-12 | Texture streaming budget tuning | perf | M | P3 | Balance VRAM use vs pop-in. |
| PERF-13 | LOD system review | perf | L | P3 | Distance/screen-size LOD selection quality and cost. |
| PERF-14 | Async resource loading | perf | L | P2 | Background streaming to cut level-load and traversal hitches. |

### Epic GFX — Rendering features / modernisation `[Track F for backend]`

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| GFX-01 | DX12 or Vulkan backend (R5) | feature | XL | P3 | Strategic; gated behind TEST-01 and Epic RND completion. |
| GFX-02 | Upscaling: FSR2/3 | feature | L | P2 | Biggest perceived-perf win for users; FSR is open and vendor-agnostic. |
| GFX-03 | Upscaling: DLSS / XeSS | feature | L | P3 | Vendor SDKs; after FSR proves the integration point. |
| GFX-04 | Modern TAA | feature | L | P2 | Better temporal stability than current AA options. |
| GFX-05 | HDR10 tone-mapping refinements | feature | M | P2 | Now that HDR10 works on both DX10/11 (this release), expose paper-white nits + curve config. |
| GFX-06 | Variable-rate shading (VRS) | feature | L | P3 | DX12/Tier-2 hardware; perf via reduced shading in low-detail regions. |
| GFX-07 | Improved shadow filtering (PCSS/contact) | feature | L | P3 | Softer, more grounded shadows. |
| GFX-08 | SSR quality pass | feature | M | P3 | Screen-space reflection stability/range. |
| GFX-09 | Volumetric fog improvements | feature | M | P3 | Density/scattering quality. |
| GFX-10 | GTAO over HBAO | feature | M | P3 | Triage notes HBAO per-subsample limitation; GTAO is higher quality. |
| GFX-11 | Bloom quality pass | feature | S | P3 | Bloom passes were touched this release; revisit threshold/spread. |
| GFX-12 | 8-bit output dithering | feature | S | P3 | Reduce banding in gradients on SDR. |
| GFX-13 | Ray-traced GI / reflections | feature | XL | P3 | Far horizon; depends on GFX-01. |

### Epic DISP — Display / output / window

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| DISP-01 | VRR (G-Sync/FreeSync)-friendly pacing | feature | M | P2 | Pair with PERF-11; avoid tearing/judder on VRR panels. |
| DISP-02 | Configurable frame-rate limiter | feature | S | P2 | In-engine cap; better than driver-side for frame pacing. |
| DISP-03 | Ultrawide / arbitrary aspect correctness | bug | M | P2 | HUD/FOV/UI scaling at 21:9, 32:9. |
| DISP-04 | Borderless windowed improvements | feature | M | P3 | Fast alt-tab, correct present mode. |
| DISP-05 | Multi-monitor handling | bug | M | P3 | Correct adapter/output selection and fullscreen target. |
| DISP-06 | DPI / UI scaling | feature | M | P3 | Crisp UI at high-DPI and 4K. |
| DISP-07 | FOV configuration (per-aspect) | feature | S | P2 | Common QoL request; compute FOV from aspect. |
| DISP-08 | Windowed refresh-rate follow-up | tech-debt | S | P3 | This release set `{0,0}`; verify behaviour across exclusive/borderless transitions. |

### Epic BUILD — Build system & CI

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| BUILD-01 | Register a Windows act-runner (`windows-latest`) | infra | M | P1 | Current release blocker — CI can't build Windows binaries without it. |
| BUILD-02 | Register a Linux runner (`ubuntu-latest`) | infra | S | P1 | Orchestration/checks/release jobs. |
| BUILD-03 | Re-enable MSVC presets in release matrix | infra | S | P2 | Match shipped toolchain to dev toolchain. |
| BUILD-04 | ccache/sccache in CI | infra | S | P2 | Cut CI build time; CMake already wires ccache if present. |
| BUILD-05 | PCH / unity-build audit | perf | M | P3 | Reduce local + CI compile time. |
| BUILD-06 | Build-time profiling | infra | M | P3 | `-ftime-trace` / MSVC build insights to find slow TUs. |
| BUILD-07 | Pin Externals submodule revisions | infra | S | P2 | Reproducibility; avoid surprise upstream drift. |
| BUILD-08 | Reproducible builds | infra | M | P3 | Deterministic output for verification. |
| BUILD-09 | `dev` integration branch + branch protection | infra | S | P2 | Prevent accidental releases on push to `cmake`. |
| BUILD-10 | Automated changelog from commits | infra | S | P3 | Generate release notes from conventional commits. |
| BUILD-11 | Symbol/PDB archival per release | infra | S | P2 | Keep PDBs to symbolicate crash dumps (pairs with Epic CRASH). |
| BUILD-12 | Fix `vswhere.exe not recognized` warning | infra | S | P3 | Benign now, but noisy in every local build log. |

### Epic CODE — Code health / modernisation `[partial Track A]`

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| CODE-01 | Raise warning level, clear warnings | tech-debt | L | P2 | Then keep it `-Werror`-clean in CI. |
| CODE-02 | Selective C++20 adoption | tech-debt | L | P3 | Where it improves safety/clarity, not for its own sake. |
| CODE-03 | Engine-wide dead/commented-code sweep | tech-debt | L | P2 | The `/* DX10 cut */` pattern recurs beyond the render layer. |
| CODE-04 | Replace crash-stubs (`VERIFY(!"...")`) | tech-debt | M | P2 | Audit all "not implemented" asserts on hot paths (like `set_AlphaRef` was). |
| CODE-05 | const-correctness pass | tech-debt | M | P3 | Incremental; improves optimisability and intent. |
| CODE-06 | Enable IWYU (currently "Not Found") | infra | S | P3 | Build log shows IWYU unconfigured; wire it to trim header deps. |
| CODE-07 | clang-format baseline | infra | M | P3 | Codify the existing tab/space rules the CI already half-enforces. |
| CODE-08 | Smart-pointer adoption in new code | tech-debt | M | P3 | RAII for COM/resources; the screenshot leak was a manual-Release miss. |

### Epic MEM — Memory

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| MEM-01 | Confirm 64-bit / Large-Address-Aware | tech-debt | S | P2 | X-Ray's classic constraint; verify current bitness and address space. |
| MEM-02 | Allocator review (mimalloc/rpmalloc) | perf | M | P3 | Faster, less-fragmenting general allocator. |
| MEM-03 | Leak-detection tooling | infra | M | P3 | Track COM/heap leaks (this release fixed one screenshot leak). |
| MEM-04 | In-game memory budget HUD | feature | S | P3 | Surface VRAM/RAM use for tuning. |
| MEM-05 | Long-session fragmentation study | perf | M | P3 | Measure drift over hours of play. |

### Epic PLAT — Platform

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| PLAT-01 | Linux/Proton (DXVK) hardening `[Track E]` | feature | L | P2 | Large Anomaly audience plays via Proton; test + fix DX11-under-DXVK issues. |
| PLAT-02 | Steam Deck verified profile | feature | M | P3 | Control scheme + default settings for the Deck. |
| PLAT-03 | Native Linux feasibility study | docs | M | P3 | Scope the Win32 dependency surface before committing. |
| PLAT-04 | ARM64 Windows feasibility | docs | S | P3 | Forward-looking; assess toolchain/SIMD gaps. |

### Epic INPUT — Input

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| INPUT-01 | Full gamepad/controller support | feature | L | P2 | First-class pad input, not just keyboard emulation. |
| INPUT-02 | Input remapping UI | feature | M | P2 | Rebindable controls in-engine. |
| INPUT-03 | Raw input / mouse-accel correctness | bug | S | P2 | Consistent, accel-free aiming. |
| INPUT-04 | Steam Input integration | feature | M | P3 | Community config support, Deck synergy. |

### Epic AUDIO — Audio

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| AUDIO-01 | OpenAL Soft + HRTF | feature | M | P2 | Modern spatialisation; binaural for headphones. |
| AUDIO-02 | Positional audio improvements | feature | M | P3 | Occlusion/reverb fidelity. |
| AUDIO-03 | Audio-device hot-swap handling | bug | S | P2 | Survive default-device changes without a restart. |
| AUDIO-04 | Selectable audio backend | feature | S | P3 | WASAPI/OpenAL choice. |

### Epic PHYS — Physics

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| PHYS-01 | Physics threading | perf | L | P3 | Move physics off the main thread. |
| PHYS-02 | ODE modernisation / replacement study | docs | M | P3 | Evaluate updating ODE vs alternatives. |
| PHYS-03 | Ragdoll / vehicle physics fixes | bug | M | P3 | Long-standing X-Ray jank. |
| PHYS-04 | Deterministic physics option | perf | L | P3 | Supports TEST-10 and netcode. |

### Epic MOD — Modding / DLTX / tooling

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| MOD-01 | DLTX performance/feature pass | feature | M | P2 | DLTX is a headline feature of this fork; profile and extend it. |
| MOD-02 | Expand in-engine ImGui dev tools | feature | M | P2 | The engine already embeds ImGui; build render/AI/memory inspectors. |
| MOD-03 | Hot-reload configs/shaders | feature | M | P2 | Massive modder iteration-speed win. |
| MOD-04 | Modder-facing engine API docs | docs | M | P3 | Document the Lua/engine boundary. |
| MOD-05 | Lua script debugging tools | feature | L | P3 | Breakpoints/inspection for script authors. |
| MOD-06 | Asset validation tooling | infra | M | P3 | Catch bad mod assets before they crash the engine. |

### Epic CRASH — Stability / diagnostics

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| CRASH-01 | Modern crash handler + minidumps | feature | M | P1 | Capturable, symbolicatable dumps on crash. |
| CRASH-02 | Symbol server for release PDBs | infra | M | P2 | Pairs with BUILD-11; makes dumps actionable. |
| CRASH-03 | Opt-in crash telemetry | feature | M | P3 | Aggregate failure signatures (consent-gated). |
| CRASH-04 | Friendly in-game error reporting | feature | S | P3 | Replace raw asserts with actionable dialogs. |
| CRASH-05 | Hang watchdog | feature | M | P3 | Detect and report main-thread stalls. |

### Epic SEC — Security / robustness

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| SEC-01 | Audit crypto module | tech-debt | M | P3 | `xr_dsa`/`xr_sha` — confirm algorithms/usage are still appropriate. |
| SEC-02 | Harden untrusted asset/config parsing | bug | L | P2 | Mods are untrusted input; fuzz the loaders (ltx, db, mesh). |
| SEC-03 | Save-file integrity validation | feature | M | P3 | Detect corrupt/tampered saves gracefully. |
| SEC-04 | Network protocol hardening | bug | M | P3 | `xrNetServer` input validation if MP paths are used. |

### Epic A11Y — Accessibility & comfort

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| A11Y-01 | Colourblind modes | feature | M | P3 | Palette options for HUD/markers. |
| A11Y-02 | Subtitle/caption support | feature | M | P3 | Dialogue and key SFX captions. |
| A11Y-03 | Configurable HUD scale | feature | S | P2 | Pairs with DISP-06; readability at 4K. |
| A11Y-04 | Comfort options (headbob/FOV/motion) | feature | S | P2 | Reduce motion sickness. |
| A11Y-05 | Photosensitivity safeguards | feature | S | P3 | Dampen rapid flashing (e.g. anomalies, NV). |

### Epic DOCS — Documentation

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| DOCS-01 | Architecture overview (render pipeline + threading) | docs | M | P2 | The single most useful onboarding artefact. |
| DOCS-02 | Contributor onboarding guide | docs | S | P2 | Build, run, debug, submit — end to end. |
| DOCS-03 | Per-subsystem READMEs | docs | M | P3 | xrCore/xrEngine/xrGame/render layers. |
| DOCS-04 | Modder API reference | docs | M | P3 | Overlaps MOD-04; the engine↔Lua surface. |
| DOCS-05 | "Why R3≈R4" mirror-maintenance note | docs | S | P3 | Codify the pairing rule so contributors keep them in sync. |

### Suggested epic ordering (value vs foundation)

```
Foundation : TEST-01/02 (safety net) → BUILD-01/02 (CI can build) → CRASH-01 (visibility)
Correctness: Epic RND (finish the port) → CODE-03/04 (kill stubs/dead code)
Perceived  : GFX-02 (FSR) → GFX-05 (HDR refine) → DISP-03/07 (ultrawide/FOV) → INPUT-01/02
Throughput : PERF-01 (profile) → PERF-05 (threaded submit) → PERF-09/14 (caches/streaming)
Strategic  : PLAT-01 (Proton) → GFX-01 (modern backend) → GFX-13 (RT)
```

### Epic AI — A-Life & AI

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| AI-01 | A-Life simulation profiling | perf | L | P2 | A-Life is STALKER's signature system and a major CPU sink; measure offline/online cost. |
| AI-02 | A-Life LOD / scheduling budget | perf | L | P2 | Time-slice NPC updates by distance/importance to bound frame cost. |
| AI-03 | Pathfinding (level/detail graph) review | perf | L | P3 | Graph build/query cost; cache and parallelise queries. |
| AI-04 | Squad/group behaviour fixes | bug | M | P3 | Long-standing coordination jank. |
| AI-05 | AI threading model | perf | XL | P3 | Move AI off the main thread; depends on determinism (TEST-10). |
| AI-06 | A-Life persistence/serialisation robustness | bug | M | P3 | Guards against save bloat/corruption from offline simulation. |
| AI-07 | Debug visualisation for AI/pathing | feature | M | P3 | ImGui overlay (pairs with MOD-02) for graphs, squads, targets. |

### Epic ENV — Environment / weather / world

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| ENV-01 | Dynamic weather system review | feature | M | P3 | Transitions, blending, mod-configurability. |
| ENV-02 | Time-of-day / lighting transition quality | feature | M | P3 | Smooth sun/sky interpolation (sun-colour gate fixed this release touches here). |
| ENV-03 | Level streaming / seamless transitions | perf | XL | P3 | Reduce loading screens between connected areas. |
| ENV-04 | World-detail (grass/vegetation) rendering | perf | L | P2 | Vegetation is a heavy GPU cost in STALKER; instancing + LOD + wind. |
| ENV-05 | Water rendering modernisation | feature | M | P3 | Reflections/refraction/quality. |

### Epic SUBSYS — Render subsystems (particles / decals / HUD)

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| SUBSYS-01 | `xrParticles` modernisation | feature | M | P3 | Throughput and visual quality of particle effects. |
| SUBSYS-02 | Decal / wallmark system review | perf | M | P3 | `script_wallmarks` exists; bound count and overdraw. |
| SUBSYS-03 | Weapon/HUD render path review | bug | M | P3 | `player_hud`/HUD items; correctness at varied FOV/aspect (ties to DISP-03/07). |
| SUBSYS-04 | Dynamic-light count/quality scaling | perf | M | P3 | Cap and prioritise dynamic lights in dense scenes. |
| SUBSYS-05 | Wind/foliage animation | feature | S | P3 | Cheap motion to bring the world alive (pairs with ENV-04). |

### Epic NET — Networking / multiplayer

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| NET-01 | `xrNetServer` code review | tech-debt | M | P3 | Modernise/validate even if MP is secondary for Anomaly. |
| NET-02 | Protocol hardening | bug | M | P3 | Overlaps SEC-04; validate all wire input. |
| NET-03 | Netcode determinism alignment | perf | L | P3 | Depends on TEST-10/PHYS-04 if MP is revived. |

### Epic I18N — Localization

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| I18N-01 | UTF-8 text pipeline end-to-end | feature | M | P3 | Robust non-Latin text handling. |
| I18N-02 | Font/glyph atlas coverage | feature | M | P3 | Cyrillic/CJK glyph support for community translations. |
| I18N-03 | String externalisation audit | tech-debt | M | P3 | No hard-coded user-facing strings. |

### Epic CFG — Settings / config / console

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| CFG-01 | Settings/options UI overhaul | feature | M | P2 | Discoverable graphics/input/audio settings (surface `ps_r4_hdr10_on` etc.). |
| CFG-02 | Per-GPU default presets | feature | M | P3 | Sensible defaults detected from hardware. |
| CFG-03 | Developer console improvements | feature | S | P3 | Autocomplete, history, cvar help. |
| CFG-04 | Config validation + migration | bug | M | P3 | Repair/migrate stale `user.ltx` across versions. |

### Epic DIST — Distribution / updates

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| DIST-01 | Versioned, automated release packaging | infra | M | P2 | Formalise the `STALKER-Anomaly-modded-exes_<date>.zip` flow once CI runners exist (BUILD-01/02). |
| DIST-02 | In-app update check/notification | feature | M | P3 | Surface new releases to users. |
| DIST-03 | Delta/patch distribution | feature | L | P3 | Ship diffs rather than full archives. |
| DIST-04 | Release-asset provenance/signing | infra | M | P3 | Sign binaries; the current zip ships a `PROVENANCE.txt` as an interim. |

### Epic REPLAY — Demo / replay / capture

| ID | Title | Class | Effort | Pri | Notes |
|----|-------|-------|--------|-----|-------|
| REPLAY-01 | Deterministic replay system | feature | XL | P3 | Record/playback inputs; depends on TEST-10. Powers regression and bug repro. |
| REPLAY-02 | Built-in video/screenshot capture tooling | feature | M | P3 | Beyond the async-screenshot fix — proper capture UX. |
| REPLAY-03 | Free-cam / photo mode | feature | M | P3 | Community-requested; also useful for TEST-01 camera capture. |

---

> This backlog is intentionally over-complete: **25 epics, ~130 tickets**. Most P3 items
> are "someday/maybe" — the point is to capture every idea so nothing is lost, then triage
> into sprints. Pull tickets into `docs/render-todo-triage.md` or a sprint plan when
> scheduled. Append new ideas with fresh IDs; never renumber existing ones.
