# xrCore — Core Engine Library

Foundation library shared by all X-Ray Monolith modules. Every other project
links against xrCore and can use its services without additional setup beyond
`#include <xrCore.h>`.

## Responsibilities

| Area | Key files | Notes |
|------|-----------|-------|
| **Math** | `_vector3.h`, `_matrix.h`, `_quaternion.h`, `_color.h`, `_compressed_normal.*` | Column-major `Fmatrix`/`Dmatrix`, SSE-aligned vectors, HSV/RGBA colour helpers |
| **Memory** | `xrMemory.h/.cpp`, `xrPool.h` | Custom `xr_new`/`xr_delete` wrappers; pool allocator for small hot objects |
| **Strings** | `xrstring.h`, `xr_string.h` | `shared_str` — ref-counted interned string; `xr_string` — `std::string` typedef; `xr_sprintf` printf wrapper |
| **File system** | `LocatorAPI.h`, `FS.h`, `FileSystem.h` | Virtual FS with mount-point aliases (`$app_data$`, `$logs$`, `$game_config$`, etc.); loose + pack-file (`.db`/`.xdb`) backend |
| **Logging** | `log.h` | `Msg()` / `Dumpf()` — thread-safe ring log flushed to disk. `FlushLog()` drains the ring. |
| **Debug / crash** | `xrDebug.h`, `xrDebugNew.cpp` | `Debug` global (`xrDebug`); `VERIFY`/`R_ASSERT`/`FATAL` macros; `save_mini_dump()` (minidump writer); hang-watchdog hook (`force_dump_and_exit`); `xr_StackWalker` for in-log stack traces |
| **Threading** | `xrCriticalSection.h`, `Lock.h`, `ScopeLock.h`, `thread_utils.h` | Thin Win32 wrappers; `xrCriticalSection` with optional profiling |
| **Timing** | `FTimer.h` | `CTimer` — QPC-based high-resolution timer |
| **Ini / config** | `Xr_ini.h`, `xrCore.h` | `CInifile` — section/key ini parser used everywhere for game config and shader params |
| **Compression** | `LzHuf.cpp`, `PPMd.h`, `xrCompression.h` | LZH + PPMd codecs; used for pack-file streaming |
| **Crypto** | `crypto/xr_sha.h`, `crypto/xr_dsa.h` | DSA-1024 + SHA-1 signing/verification (legacy primitives; see `docs/security-crypto-audit.md`) |

## Key types to know

- `shared_str` — prefer over raw `const char*` for strings that cross module boundaries; identity comparison is pointer equality.
- `CInifile` — passed everywhere as `pSettings`/`pGameIni`; sections + key/value pairs, `r_string`/`r_float`/`r_bool` accessors.
- `string_path` / `string256` / `string4096` — fixed-width stack-allocated char arrays (`typedef char string_path[512]`, etc.).
- `xr_FS` / `FS` — the global `CLocatorAPI` instance; check `xr_FS && FS.m_Flags.test(CLocatorAPI::flReady)` before using it at crash time.

## Adding new code

- New small objects → use `xr_new`/`xr_delete` (hooks memory tracking).
- New log output → `Msg("* [subsystem] ...")` prefix convention; `!` prefix for warnings, `~` for verbose.
- New cross-module API → export with `XRCORE_API`; keep headers in `src/xrCore/` and add to the CMakeLists target sources.
- New ini sections/keys → document in `docs/modding-api.md`.

## Build

Part of `XRay.Core` target in the Ninja multi-config CMake build. Compiles as a
DLL (`XRCORE_EXPORTS` defined). All other targets link `XRay.Core`.
