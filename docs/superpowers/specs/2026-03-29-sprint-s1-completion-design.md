# Sprint s1 Completion — Design Spec

**Date:** 2026-03-29
**Branch:** sprint/s1
**Author:** Holly / MuttNET

---

## Context

`sprint/s1` contains 13 commits of render cleanup and bug-fixing work. All
major groups (A, B, C, D, E, F, G, H) have been addressed to some degree.
This spec covers the remaining work required to fully close the sprint before
merging to `cmake`.

---

## Remaining Work

Six logical units, each producing one commit.

---

### Unit 1 — Triage Doc Update

**File:** `docs/render-todo-triage.md`

Update the triage document to reflect the current resolved state of all groups:

| Group | New status |
|-------|-----------|
| A | ✓ RESOLVED — 14 sites fixed in `05b96961` |
| E | ✓ RESOLVED — `t_ss_async` format matched to swapchain in `527180c6` |
| G | ✓ RESOLVED — stale MSAA TODO comments removed in `3a92cc8a`; blenders were already implemented |
| H | ✓ CLEANUP DONE (`3d8bc3fe`) — remaining real issues documented as knowledge comments (see Unit 6) |
| I | ✓ CLEANUP DONE — remaining items converted to knowledge comments (see Unit 6) |

---

### Unit 2 — 60 Hz Windowed Refresh Rate

**File:** `src/Layers/xrRenderDX10/dx10HW.cpp`
**Sites:** Two — initial swapchain creation (~line 410) and reset path (~line 707)

**Problem:** When `bWindowed == true`, `RefreshRate` is hardcoded to `{60, 1}`
for both DX10 and DX11 paths. DXGI interprets `{0, 0}` as "use the current
desktop refresh rate", which is the correct value for windowed mode — the
refresh rate field is ignored in windowed mode but `{0, 0}` is the documented
neutral value. Hardcoding 60 is misleading and will trigger validation warnings
on displays not running at 60 Hz.

**Fix:** Replace both `{60, 1}` / `{1}` windowed assignments with `{0, 0}`.
The fullscreen paths already use `selectRefresh()` and are correct — leave them
unchanged. The `// TODO: fix this, shouldn't just default to 60hz` inline
comment at line ~410 and the `// TODO: fix` at line ~708 are removed as part
of this change (they are on the same lines being replaced).

---

### Unit 3 — `DoAsyncScreenshot` Resource Leak

**Files:**
- `src/Layers/xrRenderPC_R4/r4_rendertarget_phase_combine.cpp` (DX11 path)
- `src/Layers/xrRenderPC_R3/r3_rendertarget_phase_combine.cpp` (DX10 path)

Both files contain identical `DoAsyncScreenshot` implementations with the same
defects.

**Problem:** `DoAsyncScreenshot` calls `HW.m_pSwapChain->GetBuffer(...)` which
increments the COM reference count on `pBuffer`, but never calls
`pBuffer->Release()`. Every screenshot leaks one texture reference. Additionally,
the return value `hr` is assigned but never checked — a failed `GetBuffer` call
would leave `pBuffer` null and cause `CopyResource` to crash.

The R4 (DX11) file additionally uses `__uuidof(ID3D10Texture2D)` rather than
`__uuidof(ID3DTexture2D)` — under DX11 these GUIDs differ and `GetBuffer` would
return `E_NOINTERFACE`. In R3 (DX10), `ID3DTexture2D` is typedef'd to
`ID3D10Texture2D` so the GUID is already correct; the `__uuidof` fix applies to
R4 only. The `TODO: fox that later` stale typo-comment inside `DoAsyncScreenshot`
(both files, ~line 11) should also be removed as part of this unit.

**Fix — R4:**
1. Change `__uuidof(ID3D10Texture2D)` → `__uuidof(ID3DTexture2D)`
2. Add `VERIFY(SUCCEEDED(hr))` after `GetBuffer`
3. Add `pBuffer->Release()` after `CopyResource`
4. Remove `// TODO: fox that later` comment

**Fix — R3:**
1. Add `VERIFY(SUCCEEDED(hr))` after `GetBuffer`
2. Add `pBuffer->Release()` after `CopyResource`
3. Remove `// TODO: fox that later` comment

The `__uuidof` is correct in R3 and must not be changed.

---

### Unit 4 — Alpha Ref No-Op

**File:** `src/Layers/xrRenderDX10/dx10R_Backend_Runtime.h`

**Problem:** `CBackend::set_AlphaRef` calls `VERIFY(!"Not implemented.")`, which
crashes if ever reached. DX10+ has no `D3DRS_ALPHAREF` hardware state; alpha
cutoff is handled in shader via `clip()`. Audit shows no DX10/11 render path
calls `set_AlphaRef` — it is dead code in these paths.

**Fix:** Remove the crash. Replace the body with a documented intentional no-op:

```cpp
IC void CBackend::set_AlphaRef(u32 _value)
{
    // DX10+: no hardware alpha-ref state. Alpha test is performed in shader
    // via clip(). Callers from DX9 codepaths have no effect here.
    (void)_value;
}
```

---

### Unit 5 — Sun Color Multiply in Shaft Intensity Check

**Files:** `src/Layers/xrRenderPC_R3/r3_rendertarget.cpp`,
`src/Layers/xrRenderPC_R4/r4_rendertarget.cpp`

**Problem:** `need_to_render_sunshafts()` gates sun shaft rendering on
`m_fSunShaftsIntensity` alone. A completely black sun (zero `sun_color`) will
still pass the check and trigger shaft rendering if the intensity value is
non-zero. The effective shaft contribution is
`m_fSunShaftsIntensity × luminance(sun_color)` — both must be non-negligible.

`CEnvDescriptor` exposes `Fvector3 sun_color` (R, G, B components). The check
should skip shafts when sun color luminance is also negligible.

**Fix:** In `need_to_render_sunshafts()` in both R3 and R4, insert one line after
the existing `if (fValue < 0.0001) return false;` check and remove the stale
TODO comment above it:

```cpp
// Before (existing lines, do not change):
float fValue = E.m_fSunShaftsIntensity;
if (fValue < 0.0001) return false;

// Insert after the intensity check (new line):
if (E.sun_color.square_magnitude() < 0.0001f) return false;
```

The existing `float fValue` declaration and intensity check are not modified.
The `// TODO: add multiplication by sun color here` comment that currently
appears between them is removed. Both R3 and R4 contain identical copies of
this function body — both files must receive the same insertion.

---

### Unit 6 — Investigation TODO → Knowledge Comment Pass

**Problem:** Several `TODO` comments in the codebase describe known limitations
or platform-specific behaviours with no currently actionable fix. Leaving them
as `TODO` implies they are outstanding tasks. They should be converted to
explanatory comments (no `TODO` prefix) so they accurately communicate
the known state without implying pending work.

**Files and items:**

| File | Location | Current text | Action |
|------|----------|-------------|--------|
| `dx10HW.cpp` | ~line 147 | TODO: DWM vsync not disabled on some configs | Remove the `TODO:` prefix only. The surrounding block is already a detailed knowledge comment and should not be rewritten. Change `// TODO: On some PC configurations...` → `// Note: On some PC configurations...` |
| `dx10HW.cpp` | ~line 482 | `// TODO: SDR 10-bit?` | Remove — HDR10 path already handles colour space correctly; SDR 10-bit is out of scope |
| `dx10R_Backend_Runtime.h` | ~line 84,105,121,137 | TODO: Get statistics for G/H/D Shader change | Convert — DX10 pipeline statistics are per-draw via `ID3D10Query`; these stats were never wired up and are not required for correctness |
| `dx10R_Backend_Runtime.h` | ~line 331 | TODO: Remove triangle fan usage | Convert — DX10 has no triangle fan topology; the existing early return is intentional. A future audit of callers could add triangle list decomposition, but that is not sprint scope |
| `r4.cpp` | ~line 1109 | TODO: share the same input signatures | Convert — sharing input signatures is a valid performance optimisation; not a correctness issue |
| `r3.cpp` | ~line 996 | TODO: share the same input signatures | Same as above |
| `r4.cpp` | ~line 387 | TODO: fix hbao shader | Convert — requires shader changes beyond sprint scope |
| `r3.cpp` | ~line 363 | TODO: fix hbao shader | Same as above |
| `light_vis.cpp` (R3+R4) | ~line 57 / ~line 54 | TODO: sort for performance | Convert — light sorting is a P3 optimisation; not a correctness issue |
| `r4_rendertarget_phase_combine.cpp` | ~line 716 | TODO: avoid copy if MSAA+bloom | Convert — minor optimisation, not correctness-blocking |
| `r4_rendertarget_phase_accumulator.cpp` (R3+R4) | MSAA depth block | Long TODO block about debug layer error | **Fix, not comment**: Pass `NULL` for depth in all non-SSfx paths in both files. See below. |

**MSAA accumulator depth fix detail:**

Applies to both:
- `src/Layers/xrRenderPC_R4/r4_rendertarget_phase_accumulator.cpp`
- `src/Layers/xrRenderPC_R3/r3_rendertarget_phase_accumulator.cpp`

Both files have the identical else-branch structure (non-SSfx path). In each,
change all `u_setrt(rt_Generic_2, ...)` calls in the non-SSfx else-branch to
pass `NULL` for the depth argument, for both the MSAA and non-MSAA sub-cases:

```cpp
// Current (both MSAA and non-MSAA non-SSfx paths):
u_setrt(rt_Generic_2, NULL, NULL, HW.pBaseZB);         // non-MSAA
u_setrt(rt_Generic_2, NULL, NULL, rt_MSAADepth->pZRT); // MSAA

// Fix (both):
u_setrt(rt_Generic_2, NULL, NULL, NULL);
```

Rationale: Volumetric accumulation is additive blending — no depth write
occurs and depth read for clipping is not required for correctness. The SSfx
path already passes `NULL` for depth in all cases and explicitly notes it
resolves the MSAA debug layer error. Applying `NULL` to non-MSAA as well is
consistent and safe — passing a non-matching depth to an additive RT is
unnecessary regardless of sample count.

After this fix in R4, convert the long TODO block comment to a one-line
explanatory comment. Do the same in R3.

---

## Commit Plan

| # | Commit message prefix | Unit |
|---|----------------------|------|
| 1 | `docs(triage): mark Groups A, E, G, H, I resolved or closed` | Unit 1 |
| 2 | `fix(dx10): use DXGI {0,0} for windowed refresh rate` | Unit 2 |
| 3 | `fix(r3,r4): release pBuffer and check hr in DoAsyncScreenshot` | Unit 3 |
| 4 | `chore(dx10): replace set_AlphaRef crash with intentional no-op` | Unit 4 |
| 5 | `fix(r3,r4): gate sun shaft rendering on sun color luminance` | Unit 5 |
| 6 | `chore(render): convert remaining investigation TODOs to knowledge comments` | Unit 6 |

All commits target `sprint/s1`. No shader changes. No new files.

---

## Out of Scope

- DX10 format support check / hardware capability query at init
- Triangle fan → triangle list decomposition (requires caller audit)
- HBAO per-subsample (requires shader changes)
- Input signature sharing (performance optimisation, sprint 2+)
- Light sorting (P3)
- DWM vsync fix (platform-specific, no reliable fix identified)
