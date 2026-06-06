# xrEngine — Engine Runtime

Host process and engine runtime. Owns the main loop, render device lifetime,
scheduler, input, camera, Lua VM lifecycle, and the sequencer that drives game
state transitions. Compiled as `xr_3da.exe` (the game executable).

## Responsibilities

| Area | Key files | Notes |
|------|-----------|-------|
| **Main loop** | `device.cpp`, `x_ray.cpp` | `CRenderDevice::Run()` → `message_loop()` → `on_idle()` → `FrameMove()`. `FrameMove` fires `seqFrame` which drives all per-frame subscribers. |
| **Hang watchdog** | `device.cpp` (`mt_FreezeThread`) | Background thread resets every frame via `FreezeTimer`. If no frame for 5 s (25 s during loading), calls `Debug.force_dump_and_exit()`. |
| **Render device** | `Render.h`, `dx*.h` | `CRenderDevice` holds the D3D swap chain, back-buffer, and the per-frame sequencer. Actual rendering is in `src/Layers/`. |
| **Scheduler** | `ISheduled.h`, `xrSheduler.h` | Cooperative task scheduler; objects register as `ISheduled` and get `shedule_Update` called at a requested period. |
| **Camera** | `CameraBase.h`, `CameraManager.h`, `Effector.h` | `CCameraManager` selects the active camera; effectors (post-processing, cinematic) chain on top. |
| **Input** | `xr_input.h`, `Xr_input.cpp` | DirectInput 8 keyboard + mouse; `IInputReceiver` interface for subscribers. |
| **Console** | `xr_ioconsole.h` | In-game debug console; command registration via `IConsole_Command`. |
| **Environment** | `Environment.h` | Sky, sun, weather cycle, volumetric fog parameters. |
| **Sound** | `Feel_Sound.h`, `ISoundManager.h` | Sound receiver/emitter interfaces; implementation is in the sound DLL. |
| **Fonts / HUD** | `GameFont.h`, `CustomHUD.h` | 2D overlay rendering used by the stats and in-game UI layers. |
| **Demo record/play** | `FDemoRecord.h`, `FDemoPlay.h` | Input/camera stream capture and playback for benchmarks. |
| **Lua VM** | `ai_script_lua_extension.cpp`, `script_engine.h` | LuaJIT VM lifetime; Luabind binding registration entry points. |

## Frame execution order (simplified)

```
message_loop()
  └─ on_idle()
       ├─ FreezeTimer.Start()          ← watchdog reset
       ├─ (loading events if pending)
       └─ FrameMove()
            ├─ dwFrame++
            └─ seqFrame.Process()      ← all per-frame subscribers fire here
                 ├─ renderer
                 ├─ game level tick
                 ├─ scheduler
                 └─ HUD / overlay
```

## Threading model

| Thread | Name | Role |
|--------|------|------|
| Primary | `X-RAY Primary thread` | Win32 message loop + `FrameMove` |
| Secondary | `X-RAY Secondary thread` (`mt_Thread`) | Render submission |
| Freeze watchdog | `Freeze detecting thread` (`mt_FreezeThread`) | Hang detection + minidump |
| Discord | `X-RAY Discord thread` | Discord RPC heartbeat |

The primary and secondary threads synchronise via `mt_csEnter`/`mt_csLeave` critical sections.

## Adding new code

- New per-frame work → implement `ISheduled` and register; or subscribe to `Device.seqFrame`.
- New console commands → subclass `IConsole_Command` and register in `CConsole::Initialize`.
- New input consumers → implement `IInputReceiver` and push/pop on `pInput`.
- Engine ↔ game script boundary → add Luabind bindings in the appropriate `*_script.cpp` file in `xrGame`.

## Build

`XRay.Engine` target. Links `XRay.Core`, `XRay.CDB`, render DLLs via `xrAPI`.
