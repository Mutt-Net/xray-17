# Contributing to X-Ray Monolith Edition

## Prerequisites

- **OS**: Windows 10/11 x64
- **CMake**: 3.20 or later
- **Git**: any recent version
- **Ninja**: included with Visual Studio 2022 or installable standalone

For MSVC builds:
- **Visual Studio 2022** with the "Desktop development with C++" workload
- **Windows 11 SDK** (10.0.22000.0 or later) — required for DirectX headers

For Clang builds:
- **LLVM/Clang 20.1** — [llvm.org/releases](https://releases.llvm.org/) or via `winget install LLVM.LLVM`
- Visual Studio 2022 still required for its linker and Windows SDK

## Local Build

### Configure

Choose a configure preset:

| Preset | Compiler | Generator | Notes |
|--------|----------|-----------|-------|
| `MSVS.MSVC.Ninja` | MSVC | Ninja Multi-Config | Recommended for MSVC |
| `MSVS.MSVC.Ninja.AVX` | MSVC | Ninja Multi-Config | AVX instruction set |
| `MSVS.MSVC.MSBuild` | MSVC | MSBuild | Visual Studio solution |
| `MSVS.MSVC.MSBuild.AVX` | MSVC | MSBuild | AVX variant |
| `System.Clang` | Clang | Ninja Multi-Config | Requires LLVM on PATH |
| `System.Clang.AVX` | Clang | Ninja Multi-Config | AVX variant |

```bash
cmake --preset MSVS.MSVC.Ninja
```

Build artifacts land in `_build/<PresetName>/`.

### Build

Build a specific DX target in a configuration:

```bash
cmake --build --preset MSVS.MSVC.Ninja.RelWithDebInfo --target Anomaly.DX11
```

Available targets: `Anomaly.DX8`, `Anomaly.DX9`, `Anomaly.DX10`, `Anomaly.DX11`, `Gamedata`

Available configurations: `Debug`, `Verified`, `Profiled`, `Release`, `RelWithDebInfo`

To build all DX targets in one pass:

```bash
cmake --build --preset MSVS.MSVC.Ninja.RelWithDebInfo
```

### CCache (optional)

If `ccache` is on your PATH, CMake picks it up automatically via `CMake/XRay.CCache.cmake`.
Install via `winget install ccache` or download from [ccache.dev](https://ccache.dev).

## Code Standards

CI enforces the following on all non-`Externals`/`sdk` source files:

- **Space indentation** — no tabs in `.cmake`, `.yml`, `.md`, `.txt`, `.sh`
- **No trailing whitespace** — `.h`, `.hpp`, `.inl`, `.c`, `.cpp`, `.cmake`, `.yml`, `.md`, `.txt`, `.sh`
- **Terminating newline** — same file types as above
- **LF line endings, no BOM** — `.h`, `.hpp`, `.inl`, `.c`, `.cpp`, `.cmake`, `.yml`, `.md`, `.txt`, `.sh`
- **UTF-8 encoding** — same as above

Run checks locally before pushing (requires GNU tools — Git Bash or WSL):

```bash
# Trailing whitespace
find . \( -path ./Externals -o -path ./sdk \) -prune -o \
  -iregex '.*\.\(h\|hpp\|inl\|c\|cpp\|cmake\|yml\|md\|txt\|sh\)' -type f \
  -exec grep -lP ' +$' {} \;

# Missing terminating newline
find . \( -path ./Externals -o -path ./sdk \) -prune -o \
  -iregex '.*\.\(h\|hpp\|inl\|c\|cpp\|cmake\|yml\|md\|txt\|sh\)' -type f \
  -print | while read f; do tail -c1 "$f" | read -r _ || echo "missing newline: $f"; done
```

## Pull Request Checklist

Before opening a PR:

- [ ] All four DX targets build clean on your local preset (`DX8`, `DX9`, `DX10`, `DX11`)
- [ ] No trailing whitespace or missing newlines in changed files
- [ ] LF line endings on all changed files (Git should handle this via `.gitattributes`)
- [ ] Commit messages are clear and describe the *why*, not just the *what*
- [ ] No dead code, commented-out blocks, or `// TODO` added without a tracking issue

PRs targeting `cmake` go through the full CI matrix: MSVC Ninja + Clang, all four DX targets, all five format checks.
