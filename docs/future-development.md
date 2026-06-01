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
