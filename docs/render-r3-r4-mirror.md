# Why R3 ≈ R4 — the mirror-maintenance rule (DOCS-05)

> **The one rule:** a render change almost always lands in **two** places — `xrRenderPC_R3`
> (DX10) *and* `xrRenderPC_R4` (DX11). If you touch one, check the other. PRs that change only
> one half of a mirrored pair are the single most common source of DX10-vs-DX11 divergence bugs.

## The layout

The engine builds four renderer backends from one source tree:

| Exe | Backend | Render lib | Notes |
|-----|---------|-----------|-------|
| `AnomalyDX8.exe` | R1 / DX8 | `xrRenderPC_R1` | Legacy fixed-function-ish path |
| `AnomalyDX9.exe` | R2 / DX9 | `xrRenderPC_R2` + `xrRenderDX9` | The original deferred renderer |
| `AnomalyDX10.exe` | R3 / DX10 | `xrRenderPC_R3` + `xrRenderDX10` | Port of R2 to D3D10 |
| `AnomalyDX11.exe` | R4 / DX11 | `xrRenderPC_R4` + `xrRenderDX10` | Port of R2 to D3D11 |

Three layers matter:

1. **`Layers/xrRender/`** — code shared by *all* backends (resource manager, blenders, scene
   graph, visuals, math). Most `.cpp` here is compiled into each renderer lib with that lib's
   defines (`RENDER=`, `USE_DX10`, `USE_DX11`). `XRay.Render.Common` itself compiles almost no
   `.cpp` — it is a header-aggregation module.
2. **`Layers/xrRenderDX10/`** — the **shared DX10/DX11 base**. Both R3 and R4 compile these files.
   The `DXCommonTypes.h` alias layer (`ID3D*`, `D3D_*`, `D3Dxx_*` macros) switches each symbol
   between the D3D10 and D3D11 SDK at compile time based on `USE_DX10` / `USE_DX11`. **This alias
   layer is the highest-leverage file in the render tree** — one edit here changes both DX10 and
   DX11 simultaneously, which is usually what you want.
3. **`Layers/xrRenderPC_R3/` and `Layers/xrRenderPC_R4/`** — the per-backend leaves. These are
   **near-mirrors** of each other: R3/R4 were produced by porting R2 file-by-file, so
   `r3_rendertarget.cpp` and `r4_rendertarget.cpp` (etc.) are line-for-line similar but **not
   identical** (R4/DX11 has MSAA, UAV, compute, and SSfx paths R3/DX10 lacks).

## Where a change goes

| If the change is… | Edit… | Affects |
|-------------------|-------|---------|
| DX10 **and** DX11 logic, same code | `xrRenderDX10/` (behind `DXCommonTypes.h`) | R3 + R4 together — preferred |
| DX9, DX10, DX11 shared logic | `xrRender/` | R2 + R3 + R4 |
| R3-specific (DX10 only) | `xrRenderPC_R3/` | R3 — **then check the R4 mirror** |
| R4-specific (DX11 only: MSAA/UAV/compute/SSfx) | `xrRenderPC_R4/` | R4 only — legitimately not mirrored |

## The discipline (RND-11)

- After editing an `r3_*.cpp`, open the matching `r4_*.cpp` (and vice versa). If the fix applies,
  apply it to both; if it legitimately does not (an R4-only MSAA branch, say), note why in the
  commit.
- Prefer landing shared fixes in `xrRenderDX10/` so the mirror stays in sync by construction —
  that is why sprint s1/s2/s3 fixes mostly touched `xrRenderDX10/` rather than the leaves.
- Line numbers between the mirrors drift (R4 has extra passes), so match by **function and
  intent**, not by line.

Cross-reference: `docs/render-todo-triage.md` (issues are listed once with both R3/R4 paths),
`docs/muttcode-changes.md` (every fix records which backends it touched).
