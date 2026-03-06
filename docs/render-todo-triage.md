# Render TODO/FIXME Triage — xrRenderPC_R3 / R4 / xrRenderDX10

Generated: 2026-03-06. Scope: xrRenderPC_R3, xrRenderPC_R4, xrRenderDX10.

**Legend**
- **Class**: `bug` | `missing-feature` | `cleanup` | `unknown`
- **Priority**: `P1` visible artefact | `P2` correctness | `P3` cleanup/perf

> R3 and R4 are near-mirrors. Entries shared between both are listed once with
> both paths noted.

---

## Group A — Half-Pixel Offset (DX10 NDC correction)

**Class:** bug | **Priority:** P1

DX9 required a half-texel offset (`0.5 / width`, `0.5 / height`) to align
texels to pixels in screen-space quad passes. DX10+ corrects this in the NDC
model — the offset must be removed or it produces a 0.5px sub-pixel shift
visible in deferred lighting and combine passes.

| File (R3) | Line | File (R4) | Line | Notes |
|-----------|------|-----------|------|-------|
| r3_rendertarget.cpp | 127 | r4_rendertarget.cpp | 185 | `remove half pixel offset?` |
| r3_rendertarget_accum_direct.cpp | 48 | r4_rendertarget_accum_direct.cpp | 46 | Sun near pass |
| r3_rendertarget_accum_direct.cpp | 360 | r4_rendertarget_accum_direct.cpp | 359 | Sun far pass |
| r3_rendertarget_accum_direct.cpp | 751 | r4_rendertarget_accum_direct.cpp | 749 | Sun final pass |
| r3_rendertarget_accum_direct.cpp | 1045 | r4_rendertarget_accum_direct.cpp | 1043 | Cascade pass |
| r3_rendertarget_phase_combine.cpp | 46 | r4_rendertarget_phase_combine.cpp | 46 | phase_combine setup |
| r3_rendertarget_phase_combine.cpp | 703 | r4_rendertarget_phase_combine.cpp | 877 | combine final quad |

**Fix pattern:** Each site sets `p0.set(.5f / _w, .5f / _h)` and `p1.set((_w + .5f) / _w, ...)`.
Remove the `+ .5f` and `0.5f /` bias terms. Gate on `HW.FeatureLevel >= D3D_FEATURE_LEVEL_10_0`.

---

## Group B — DX10 Rain Not Implemented

**Class:** missing-feature | **Priority:** P1

DX10 rain render pass is completely missing. The bounding sphere calculation
and resolution reduction are unimplemented stubs, so rain doesn't render on DX10.

| File (R3) | Line | File (R4) | Line | Notes |
|-----------|------|-----------|------|-------|
| r3_R_rain.cpp | 167–168 | r4_R_rain.cpp | 172–173 | Bounding sphere + resolution |
| r3_R_render.cpp | 487 | r4_R_render.cpp | 526 | `Implement DX10 rain` |
| r3_rendertarget.cpp | 582 | r4_rendertarget.cpp | 792 | `Create resources only when rain enabled` |

---

## Group C — NV DBT / Shadow Mapping

**Class:** bug | **Priority:** P2

NV Depth Bounds Test is an NVIDIA extension unavailable on DX10. Multiple
shadow accumulation passes check for an NV DBT analogue that was never
implemented. Standard PCF is the correct replacement.

| File (R3) | Line | File (R4) | Line | Notes |
|-----------|------|-----------|------|-------|
| r3_rendertarget_accum_direct.cpp | 278 | r4_rendertarget_accum_direct.cpp | 277 | Sun near smap |
| r3_rendertarget_accum_direct.cpp | 334 | r4_rendertarget_accum_direct.cpp | 333 | — |
| r3_rendertarget_accum_direct.cpp | 631 | r4_rendertarget_accum_direct.cpp | 629 | Sun far smap |
| r3_rendertarget_accum_direct.cpp | 728 | r4_rendertarget_accum_direct.cpp | 726 | — |
| r3_rendertarget_accum_direct.cpp | 1302 | r4_rendertarget_accum_direct.cpp | 1305 | Cascade |
| r3_rendertarget_accum_direct.cpp | 1373 | r4_rendertarget_accum_direct.cpp | 1376 | — |
| r3_rendertarget_draw_rain.cpp | 216, 392 | r4_rendertarget_draw_rain.cpp | 218, 396 | Rain shadow |
| r3_rendertarget.cpp | 569, 1118 | r4_rendertarget.cpp | 779, 1339 | SMap resource creation |
| r3_rendertarget_phase_smap_D.cpp | 4 | r4_rendertarget_phase_smap_D.cpp | 4 | Old smap check |
| r3_rendertarget_phase_smap_S.cpp | 16 | r4_rendertarget_phase_smap_S.cpp | 16 | Spot smap check |

---

## Group D — Inverse Culling (Far Region)

**Class:** bug | **Priority:** P2

DX10 requires explicit rasterizer state for culling reversal; the DX9 approach
of calling `SetRenderState(D3DRS_CULLMODE, ...)` doesn't exist. These stubs
are in the far shadow region passes.

| File (R3) | Line | File (R4) | Line |
|-----------|------|-----------|------|
| r3_rendertarget_accum_direct.cpp | 169, 481, 941 | r4_rendertarget_accum_direct.cpp | 167, 480, 939 |
| r3_rendertarget_draw_rain.cpp | 121 | r4_rendertarget_draw_rain.cpp | 123 |

---

## Group E — HDR Texture Format Incompatibility (DX11)

**Class:** bug | **Priority:** P1

`r4_rendertarget_phase_combine.cpp:30` — async screenshot copy uses
`CopyResource` into `t_ss_async`, but the comment acknowledges this won't work
on DX11 with HDR due to texture format mismatch. The swap chain buffer is
`DXGI_FORMAT_R8G8B8A8_UNORM` in SDR but `DXGI_FORMAT_R16G16B16A16_FLOAT` or
`DXGI_FORMAT_R10G10B10A2_UNORM` in HDR — `CopyResource` requires matching
formats.

Related: `r4_rendertarget.cpp:564` — `R11G11B10F` format noted as a "horrible
hack".

| File | Line | Notes |
|------|------|-------|
| r4_rendertarget_phase_combine.cpp | 30 | P1 — HDR screenshot broken on DX11 |
| r4_rendertarget.cpp | 564 | P2 — R11G11B10F format approach flagged |

---

## Group F — Stencil Two-Sided / Scissor Rect

**Class:** missing-feature | **Priority:** P2

DX10 requires separate rasterizer state objects for two-sided stencil (no
`D3DRS_TWOSIDEDSTENCILMODE`). Scissor rect enable/disable also requires
rasterizer state in DX10.

| File (R3) | Line | File (R4) | Line | Notes |
|-----------|------|-----------|------|-------|
| r3_rendertarget_phase_scene.cpp | 58 | r4_rendertarget_phase_scene.cpp | 59 | Two-sided stencil |
| r3_rendertarget_enable_scissor.cpp | 47, 58 | r4_rendertarget_enable_scissor.cpp | 47, 58 | Scissor |

---

## Group G — Blender Missing Implementations

**Class:** missing-feature | **Priority:** P2

Multiple blender `Compile()` methods have stub/empty DX10 paths. These blenders
handle light accumulation and reflection — missing implementations mean those
effects silently do nothing or fall back incorrectly.

| File | Lines (R3 / R4) | Effect |
|------|-----------------|--------|
| blender_combine.cpp | R3:162 / R4:193 | Final combine |
| blender_light_direct.cpp | R3:135,271,319 / R4:143,287,335 | Direct lighting |
| blender_light_mask.cpp | R3:10,81 / R4:10,81 | Direct light mask |
| blender_light_occq.cpp | R3:10 / R4:10 | NV occlusion query opt |
| blender_light_point.cpp | R3:141 / R4:147 | Point lights |
| blender_light_reflected.cpp | R3:38 / R4:38 | Reflections |

---

## Group H — xrRenderDX10 Device Layer

**Class:** mixed | **Priority:** P2–P3

| File | Line | Class | Priority | Notes |
|------|------|-------|----------|-------|
| dx10HW.cpp | 147 | bug | P2 | Refresh rate defaults to 60hz |
| dx10HW.cpp | 232 | missing-feature | P2 | DX10 init incomplete |
| dx10HW.cpp | 369 | missing-feature | P2 | Dynamic format selection |
| dx10HW.cpp | 487 | unknown | P3 | SDR 10-bit path not considered |
| dx10HW.cpp | 713 | bug | P2 | Unspecified fix needed |
| dx10HW.cpp | 878 | cleanup | P3 | Check obsolete state needs |
| dx10HW.cpp | 1074 | missing-feature | P2 | Stub for legacy code |
| dx10ResourceManager_Resources.cpp | 177,289,389 | missing-feature | P2 | Shader HACK — all shaders must be implemented |
| dx10ResourceManager_Scripting.cpp | 49,369 | unknown | P3 | Commented-out scripting paths |
| dx10r_constants.cpp | 150 | unknown | P3 | Empty TODO |
| dx10SH_RT.cpp | 93,274 | missing-feature | P2 | Format support + cube validation |
| dx10TextureUtils.cpp | 17 | cleanup | P3 | nullrt hack |
| dx10TextureUtils.cpp | 104 | bug | P2 | D3DFMT_A2R10G10B10 ABGR/ARGB channel swap |
| dx10StateCache.cpp | 55,68,81 | cleanup | P3 | Remove DX9 state cache leftovers |
| StateManager/dx10StateManager.cpp | 8 | missing-feature | P2 | Alpha reference control |
| dx10R_Backend_Runtime.h | 11 | missing-feature | P2 | set_xform stub |
| dx10R_Backend_Runtime.h | 332 | missing-feature | P2 | Triangle fan not supported in DX10 |
| dx10R_Backend_Runtime.h | 424 | missing-feature | P2 | Alpha ref via rasterizer state |

---

## Group I — Performance / Miscellaneous

**Class:** mixed | **Priority:** P3

| File | Line | Notes |
|------|------|-------|
| r3/r4_loader.cpp | R3:248,279 / R4:235,265 | Check geometry buffer fragmentation |
| r3/r4.cpp | R3:996 / R4:1109 | Share input signatures |
| r3/r4.cpp | R3:363 / R4:387 | HBAO per-subsample |
| r3/r4_rendertarget_phase_combine.cpp | R4:718 | Avoid copy when MSAA + bloom both enabled |
| light_vis.cpp | R3:54 / R4:57 | Sort lights for performance |
| r3_rendertarget.cpp | 1240 / r4:1493 | Sun color multiplication missing |
| r4_rendertarget_phase_accumulator.cpp | 81 | MSAA + D3D debug layer error |

---

## Summary

| Priority | Count | Action |
|----------|-------|--------|
| P1 | ~18 | Sprint 2–3: half-pixel offset, rain, HDR copy |
| P2 | ~60 | Sprint 3–4: shadow mapping, stencil, blenders |
| P3 | ~20 | Sprint 6+: cleanup, perf investigation |

**Sprint 2 targets (E2-T2, E2-T3):** Group A (half-pixel offset) + Group B (rain).
**Sprint 3 targets (E2-T4, E2-T5, E2-T6):** Groups C, D, E, F.
**Sprint 4+:** Groups G, H, I.
