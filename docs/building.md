# Building X-Ray Monolith (DOCS-02)

Contributor build guide for the `cmake` line. Covers the local Windows/MSVC toolchain, the build
commands, and the failure modes you will actually hit.

## What you get

One build produces four renderer exes — `AnomalyDX8.exe` (R1), `AnomalyDX9.exe` (R2),
`AnomalyDX10.exe` (R3), `AnomalyDX11.exe` (R4) — plus AVX variants from a separate preset. The
exes are **64-bit** and linked **Large-Address-Aware** (`/machine:x64 /LARGEADDRESSAWARE`) — see
MEM-01. They need a S.T.A.L.K.E.R. Anomaly 1.5.3 install (gamedata) to run; the repo ships engine
source only.

## Toolchain

- **Visual Studio 18 Build Tools** (MSVC `cl` 19.50 / toolset 14.50). The build uses the cmake +
  Ninja that ship *inside* VS Build Tools — they are **not on `PATH`** even after `vcvars64.bat`.
  Invoke cmake by full path:
  ```
  "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  ```
- Generator: **Ninja Multi-Config**. Presets live in `CMakePresets.json`.

> History note: this tree was previously built with VS2022 (v17). If you see a build fail trying
> to launch a ninja under `...\2022\BuildTools\...`, your cache is from the old toolchain — see
> "Stale cache" below.

## Configure + build

From a shell with the MSVC environment loaded (`vcvars64.bat`), from the repo root:

```bat
cmake --preset MSVS.MSVC.Ninja
cmake --build --preset MSVS.MSVC.Ninja.RelWithDebInfo
```

A from-scratch build is ~2941 steps (externals + engine + game + four renderers). Incremental
builds after a small change recompile only the touched translation units and relink the affected
exes (the relinks of the ~21 MB exes are the slow part).

AVX variants: configure preset `MSVS.MSVC.Ninja.AVX`, build preset `...AVX.RelWithDebInfo`
(exes suffixed `AnomalyDX*AVX.exe`).

Gamedata pack (not part of `all`): build the `Gamedata` target, or — because this machine does
not resolve bare exe names in cwd — run `compressor\xrCompress.exe` manually (see the
`xray-monolith` project notes).

## Failure modes you will hit

1. **Stale cache after a toolchain change.** `CMakeCache.txt` embeds absolute toolchain paths
   (`CMAKE_MAKE_PROGRAM`, the compiler). If the VS edition changed (e.g. 2022 → 18) or the repo
   moved drives, the build dies with "system cannot find the path specified" / a missing ninja.
   **Fix:** delete `_build/<preset>/CMakeCache.txt` and `_build/<preset>/CMakeFiles/`, then
   re-run `cmake --preset`. If only the *path* changed (same compiler), object files are reused;
   if the compiler *edition* changed, expect a full rebuild.

2. **`LNK1000: Internal error during LIB::Search` / "In-page error".** Transient. This repo is
   often worked on over a UNC/network share (`\\HOST\...` mapped to a drive letter); an in-page
   error is a failed memory-mapped read of a `.lib`/`.obj` over that share. **Fix:** just re-run
   the build — the relink succeeds on retry. It is not a code fault.

3. **Stale `.git/index.lock`.** TortoiseGit's TGitCache scans the repo and can leave a stale
   `index.lock`, blocking commits. Verify no live git *write* process (TGitCache spawns
   short-lived read-only `git.exe` scans — harmless), then `rm -f .git/index.lock` and retry.

4. **Format gate.** CI runs in-place transforms (line-endings, trailing whitespace, mixed
   tabs on `.cmake/.yml/.md/.txt/.sh/.cs`, terminating newline) and fails if a file changes.
   Committed files are LF. C++ keeps tabs; markdown/yaml use spaces. Don't add trailing
   whitespace.

## Verifying a change

- Build the four targets clean (`BUILD_EXIT == 0`, four `Anomaly*.exe` produced).
- There is **no** automated render-regression test (that harness was mothballed — see
  `tools/render_regression/`); render changes are validated by eye against the running game.
- The comparator there (`compare.py --selftest`) is the only render-side unit test and runs
  without a GPU.

See also: `docs/render-r3-r4-mirror.md` (where render changes go), `docs/render-todo-triage.md`,
`docs/future-development.md`.
