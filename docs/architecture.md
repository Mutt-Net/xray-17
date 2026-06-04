# Architecture Overview (DOCS-01)

A map of the engine for new contributors: the module layout, the frame loop / threading model,
and the render pipeline. Pairs with `docs/building.md` (how to build) and
`docs/render-r3-r4-mirror.md` (where render changes go).

## Module map (`src/`)

| Module | Role |
|--------|------|
| `xrCore` | Foundation: memory, strings, math (`Fmatrix`/`Fvector`), FS/archives (`.db`), LTX/DLTX config, threading primitives, RTTI helpers. Everything depends on it. |
| `xrEngine` | The engine shell: device + main loop (`device.cpp`), input, console + cvars, render-device abstraction (`Render->`), demo record/play, environment/weather, UI host. Renderer-agnostic. |
| `Layers/` | The renderers. `xrRender` (shared), `xrRenderDX9`, `xrRenderDX10` (shared DX10/11 base), `xrRenderPC_R1/R2/R3/R4`. See **Render pipeline** below. |
| `xrGame` | Gameplay: A-Life, AI (monsters/stalkers), inventory, weapons, UI screens, scripting bridge (Lua/luabind). The largest module. |
| `xrPhysics` | ODE-based physics: ragdolls, vehicles, world collision response. |
| `xrCDB` | Collision database — static-geometry AABB tree for fast ray/box queries. |
| `xrSound` | Audio backend + 3D positional sound. |
| `xrParticles` | Particle-system simulation (visuals live in the render layer). |
| `xrNetServer` / `xrServerEntities` | Multiplayer server + networked entity serialization. |
| `xrCPU_Pipe`, `xrXMLParser`, `xrPlatform*` | SIMD helpers, XML (UI configs), platform abstraction. |
| `Externals/` | Vendored third-party as git submodules (LuaJIT, imgui, OpenAL, ogg/vorbis/theora, optick, …). See `docs/submodules.md`. |

Dependency direction: `xrCore` ← everything; `xrEngine` ← game + renderers; a renderer lib is
selected at link time to produce each `AnomalyDX*.exe`.

## Frame loop & threading

The engine runs a **two-thread** core (plus two helper threads), spawned in
`CRenderDevice::Run()` (`device.cpp`):

| Thread | Name | Job |
|--------|------|-----|
| Primary | `"X-RAY Primary thread"` | The main loop: input, `seqFrame`, game logic, render submission. |
| Secondary | `"X-RAY Secondary thread"` (`mt_Thread`) | Runs `seqFrameMT` and `seqParallel` work in parallel with the primary. |
| Watchdog | `"Freeze detecting thread"` (`mt_FreezeThread`) | Detects main-thread hangs. |
| Discord | `"X-RAY Discord thread"` | Discord Rich Presence, off the hot path. |

**Per-frame sequences** (registries of callbacks, `device.h`):

- `seqFrame` (`pureFrame`) — primary-thread frame callbacks (most subsystems).
- `seqFrameMT` (`pureFrame`) — callbacks run on the **secondary** thread, overlapped with the
  primary's frame.
- `seqRender` (`pureRender`) — render callbacks during the render phase.
- `seqParallel` — one-shot `FastDelegate` tasks drained each frame by whichever thread gets there.

**Synchronisation:** a single critical section, `mt_csEnter`, rendezvouses the primary and
secondary threads each frame. The secondary thread enters, processes `seqFrameMT` + `seqParallel`,
and stamps `mt_Thread_marker = dwFrame`. If the secondary thread did **not** reach the current
frame (`dwFrame != mt_Thread_marker`), the primary drains `seqParallel` itself as a fallback. This
is the classic X-Ray overlap model — cheap parallelism without a full job system — and the reason
adding work to `seqFrameMT` must be thread-safe against the primary thread.

`fTimeDelta` is computed and smoothed in the loop (clamped so a stall reads as ≥15 fps minimum).

## Render pipeline

A **deferred renderer** built four ways from one tree (R1/DX8, R2/DX9, R3/DX10, R4/DX11). The
engine talks to whichever backend is linked through the `Render->` interface (`xrEngine/Render.h`);
the backend implements scene traversal → G-buffer fill → lighting accumulation → combine →
post-processing.

Layering (full detail in `docs/render-r3-r4-mirror.md`):

- `Layers/xrRender/` — shared across all backends (resource manager, blenders, scene graph,
  visuals, math). Compiled into each renderer lib with that lib's defines.
- `Layers/xrRenderDX10/` — the shared DX10/DX11 base; `DXCommonTypes.h` aliases switch each symbol
  between the D3D10 and D3D11 SDKs at compile time. **Highest-leverage file in the tree.**
- `Layers/xrRenderPC_R3` / `R4` — per-backend leaves; near-mirrors (R4/DX11 adds MSAA, UAV,
  compute, SSfx that R3/DX10 lacks).

Resource lifetime flows through `CResourceManager` (`xrRender/ResourceManager*.cpp`): textures,
render targets (`CRT`), cube RTs (`CRTC`, DX10+ only), shaders, constants, and state objects.
DX10/11 state is cached via the `dx10StateCache` / `dx10StateManager` layer (immutable state
objects, unlike DX9's per-call state).

## Where to look first

- A render bug → `Layers/xrRenderDX10/` + the matching `r3_*`/`r4_*` leaves; check the mirror.
- A frame-timing / threading issue → `xrEngine/device.cpp` (`mt_Thread`, the `seq*` sequences).
- A config/cvar → `xrEngine` console + `xrCore` LTX/DLTX.
- Gameplay/AI → `xrGame` (+ the Lua boundary).

Cross-references: `docs/building.md`, `docs/render-r3-r4-mirror.md`,
`docs/render-todo-triage.md`, `docs/future-development.md`.
