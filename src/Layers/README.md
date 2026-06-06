# src/Layers — Render Layers

The render pipeline is split across several layers. Each layer is a separate
DLL loaded at runtime by `xrAPI` based on the renderer chosen by the user
(`r1`/`r2`/`r3`/`r4` console variable).

## Layer map

```
xrAPI/               Renderer-agnostic interface (IRender, IResourceManager, …)
xrRender/            Shared render code (resource manager, blenders, constants, shaders)
xrRenderDX9/         DX9 backend utilities (state cache, HW caps for R1/R2)
xrRenderPC_R1/       R1 — forward renderer, DX8 feature level
xrRenderPC_R2/       R2 — deferred lighting, DX9 feature level
xrRenderDX10/        DX10/11 backend utilities (constant buffers, resource reflection)
xrRenderPC_R3/       R3 — deferred shading, DX10 feature level
xrRenderPC_R4/       R4 — deferred shading + DX11 extras (tessellation, CS)
```

R3 and R4 are near-mirrors of each other. Any bug fix or feature added to one
almost always applies to the other. See `docs/render-r3-r4-mirror.md` for the
mirroring rule and how to keep them in sync.

## xrAPI — Render abstraction

`xrAPI/` declares the pure-virtual interface that the engine uses to talk to the
renderer (`IRender`, `IRenderVisual`, etc.). At startup, the engine loads one of
the four render DLLs and calls `CreateInterfaceFactory` to get the concrete
`IRender` implementation. No engine code above `xrAPI` should `#include` render
internals directly.

## xrRender — Shared render code

Code used by all four renderers:

| File/group | Purpose |
|------------|---------|
| `ResourceManager_*.cpp` | Shader/texture/RT resource cache; `CreateTexture`, `CreateBlender`, `CreateShader` |
| `r_constants.h/.cpp` | Shader constant table parsing (`R_constant_table::parse`) for DX9 |
| `Blender_*.cpp` | Base blender classes (combine, deferred, light accumulation) |
| `HOM.cpp` | Hierarchical occlusion map |
| `DetailManager.cpp` | Grass/detail-object streaming and rendering |

## xrRenderDX10 — DX10/11 utilities

Provides the DX10/11-specific implementations of resource management, constant
buffer reflection, and state management that R3/R4 share:

| File | Purpose |
|------|---------|
| `dx10r_constants.cpp` | `R_constant_table::parseConstants` / `parseResources` — HLSL SM4+ reflection via `ID3DShaderReflection`; handles cbuffer scalars/vectors/matrices and resource bindings (textures, samplers, UAVs) separately |
| `dx10HW.cpp` | DXGI adapter enumeration, swap-chain creation, feature-level detection, HDR10 backbuffer format |
| `dx10ConstantBuffer.h/.cpp` | `dx10ConstantBuffer` — typed cbuffer wrapper with dirty tracking |
| `dx10ResourceManager_*.cpp` | DX10/11 texture/shader/RT resource management |
| `dx10StateCache.cpp` | DX10 pipeline state cache (`#ifdef DEBUG` state-creation logging) |

## xrRenderPC_R3 / xrRenderPC_R4

Per-renderer source that references the shared `xrRenderDX10` and `xrRender` bases:

| File pattern | Purpose |
|--------------|---------|
| `r3.cpp` / `r4.cpp` | Top-level renderer: `Render()`, scene setup, input-signature sharing |
| `r3_rendertarget*.cpp` | G-buffer construction, deferred lighting passes, combine pass |
| `r3_loader.cpp` | Static geometry streaming and geometry-buffer upload |
| `blender_*.cpp` | R3/R4-specific blender overrides |
| `light_vis.cpp` | Light visibility and sorting |

## Adding a new render feature

1. If it applies to both R3 and R4, add it to both `r3_*.cpp` and `r4_*.cpp` in tandem and note the mirroring in your commit message.
2. If it's DX10/11 specific infrastructure, put it in `xrRenderDX10/`.
3. If it's renderer-agnostic (affects R1/R2 as well), put it in `xrRender/`.
4. Expose new shader params through `R_constant_table` — never hardcode slot indices.
5. After adding HLSL shader constants, verify with `--dxgi-dbg` that the debug layer produces no validation errors.

## Further reading

- `docs/render-todo-triage.md` — classified inventory of known render issues
- `docs/render-r3-r4-mirror.md` — R3/R4 mirroring rule
- `docs/architecture.md` — full pipeline diagram
