# Externals / submodule pins (BUILD-07)

The build's third-party dependencies are git submodules under `Externals/` (plus
`sdk/include/reshade-git`). Git already pins each to an exact commit (the gitlink SHA recorded in
the superproject tree). `.gitmodules` only records URLs, so this file makes the **pinned SHAs
auditable** in one place and states the anti-drift policy.

## Anti-drift policy

- **Never** run `git submodule update --remote` casually — it moves a submodule to its upstream
  branch tip and silently changes the build. Bumping a dependency is a deliberate, reviewed
  commit: update the one submodule, rebuild, and record the new SHA here.
- After `git clone`, initialise with `git submodule update --init --recursive` (this checks out
  the **pinned** SHAs, not branch tips).
- A submodule shown below as tracking `master`/`main` is still pinned to a specific SHA in the
  tree; the branch name is just what that SHA was the tip of at pin time. These are the
  higher-drift-risk entries to watch when bumping.

## Pinned revisions (as of 2026-06-04)

| Submodule | Pinned SHA | Tag / branch | URL |
|-----------|-----------|--------------|-----|
| Externals/WindowsToolchain | d895710e | v0.12.0-4 | MarkSchofield/WindowsToolchain |
| Externals/cximage-git | 68934af2 | main | minnyres/cximage |
| Externals/DxErr-git | e4872830 | v1.2.0 | rbeesley/DxErr |
| Externals/IconFontCppHeaders-git | 1c004c59 | (detached) | juliettef/IconFontCppHeaders |
| Externals/imgui-git | 11b3a7c8 | v1.91.8-docking | ocornut/imgui |
| Externals/libjpeg-git | f57ac58a | master | kloper/libjpeg |
| Externals/libogg-git | ab78196f | master | gcp/libogg |
| Externals/libtheora-git | 7ffd8b2e | v1.1.1 | xiph/theora |
| Externals/libvorbis-git | 84c02369 | v1.3.7-10 | xiph/vorbis |
| Externals/luafilesystem-git | 09511782 | v1_8_0-14 | lunarmodules/luafilesystem |
| Externals/LuaJIT-git | 69e5342e | v2.0.4 | LuaJIT/LuaJIT |
| Externals/lua-marshal-git | fc9451fc | master | richardhundt/lua-marshal |
| Externals/LuaPanda-git | e3ac3d33 | v3.2.0-42 | Tencent/LuaPanda |
| Externals/luasocket-git | 4844a48f | v3.1.0-47 | lunarmodules/luasocket |
| Externals/OpenAL-git | d3875f33 | 1.23.1 | kcat/openal-soft |
| Externals/optick-git | 8abd28de | 1.4.0.0-9 | bombomby/optick |
| Externals/StackWalker-git | 50a4ec59 | 1.20-26 | JochenKalmbach/StackWalker |
| Externals/FastDynamicCast-git | 1bf5c039 | master | tobspr/FastDynamicCast |
| sdk/include/reshade-git | 6582644f | v6.4.1 | crosire/reshade |

Regenerate this table with `git submodule status` (full SHAs there; truncated to 8 here).

## Notes

- **Fixed 2026-06-04:** `.gitmodules` had two stanzas for the same `Externals/optick-git` path —
  a stale `[submodule "src/3rd party/optick-git"]` left over from when optick lived under
  `src/3rd party/`, plus the canonical `[submodule "Externals/optick-git"]`. The stale stanza was
  removed (the live `.git/config` already used the canonical name). Duplicate submodule paths are
  malformed and confuse `git submodule` operations.
- Branch-tracking pins (`master`/`main`): cximage, libjpeg, libogg, lua-marshal, FastDynamicCast.
  These are the ones most worth pinning to a tag on the next dependency-bump pass.
