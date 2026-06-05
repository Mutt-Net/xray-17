# CODE-04 — crash-stub (`VERIFY(!"…not implemented")`) audit

Audit of the render layer's "not implemented" crash-stubs, classifying each as **safe to no-op**
(debug/dead path — crashing is worse than degrading), **deferred `[runtime]`** (render/gameplay
path — a blind no-op could hide a real bug or corrupt output), or **already harmless** (the
`VERIFY` is commented out — a knowledge comment).

Background: in `RelWithDebInfo`/release, `VERIFY` compiles out, so a live stub becomes whatever
follows it (often a silent wrong result); in debug it asserts. The fix pattern (see `set_AlphaRef`
in sprint s1, `set_xform` in s3) is to turn a *debug/dead* stub into an explicit, documented
no-op, while *leaving* stubs that guard a genuine gameplay/render gap so the gap stays visible.

## Converted to documented no-op (sprint s4, 2026-06-05, build-verified)

| Site | Why safe |
|------|----------|
| `dxRenderDeviceRender.cpp` `overdrawBegin` / `overdrawEnd` | Overdraw visualisation is a DX9 stencil-counting **debug** mode; callers are guarded by `if (HW.Caps.SceneMode)`. No DX10+ fixed-function stencil-state setters. No-op degrades gracefully instead of crashing on toggle. (Also fixed the copy-paste assert message.) |
| `R_Backend.h` `dbg_SetRS` / `dbg_SetSS` | Debug-only render/sampler-state setters, reached via `DU_DRAW_RS` in `D3DUtils`. Fixed-function state has no DX10+ equivalent; debug-draw now degrades instead of asserting. |

## Deferred `[runtime]` — do NOT no-op blind

| Site | Risk |
|------|------|
| `SkeletonX.h:168` `pick_bone<T>` (DX10/11) | Bone picking (ray vs skinned mesh) — **gameplay**. A silent `return FALSE` would hide broken hit-detection. Needs runtime proof of which overload is live. |
| `r3_/r4_rendertarget_phase_smap_S.cpp:38` `phase_smap_spot_tsh` | Translucent-shadow spot clear — a **render** path gated on `o.Tshadows`. Removing the stub without implementing the buffer clear could corrupt translucent shadows. |
| `dx10HW.cpp:1075` `CHW::support` | Hardware-capability query — wrong answer changes feature paths. (Group H P2.) |
| `dx10r_constants.cpp:150` shader-object parsing | = **RND-09**; implementing sampler/texture reflection needs the live shader pipeline. |

## Safe candidate — convert next pass

| Site | Note |
|------|------|
| `dxDebugRender.cpp:145` `SetAmbient` | Sets `D3DRS_AMBIENT` (fixed-function ambient) — no DX10 equivalent, debug render device. Safe to no-op; left out of the s4 pass only to keep that pass's build a verified unit. |

## Already harmless (commented — knowledge comments, no action)

`D3DUtils.cpp:99`, `dxRenderDeviceRender.cpp:105` (SetupStates),
`R_Backend_Runtime.cpp:153,194` (set_ClipPlanes), `tss_def.cpp:9,201`,
`xrRender_console.cpp:633`. These `//VERIFY(...)` lines cannot fire.

## Status

CODE-04 is **substantially done**: the genuinely-unsafe debug crash-stubs on reachable toggles
are no-op'd; the render/gameplay stubs are catalogued and intentionally left asserting until they
can be implemented + verified at runtime. Re-run the audit with:
`rg 'VERIFY\(!"' src/Layers` (ripgrep) and re-triage any new hits against the table above.
