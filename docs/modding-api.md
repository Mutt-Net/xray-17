# Modding / Engine-Lua API Orientation (DOCS-04)

A map of the **engine ↔ Lua** boundary for mod authors. This is an *orientation* reference — the
exported surface is hundreds of classes, so this documents the **mechanisms** and where to find
each part, not an exhaustive per-method list (the source `*_script.cpp` files are the canonical
reference, and stay accurate as they change).

Audience: modders writing Lua against the engine, and engine devs adding to the exported surface.
Contributor-facing architecture is in `docs/architecture.md`.

## The boundary in one paragraph

Gameplay runs partly in C++ (`xrGame`) and partly in Lua (gamedata scripts), bridged by
**luabind** over **LuaJIT** (`Externals/LuaJIT-git`). Engine C++ classes export selected methods
to Lua; Lua scripts attach to game objects via **binders** and react to engine events via
**callbacks**; configuration flows through **LTX/DLTX** files that Lua and the engine both read.

## 1. Exporting C++ to Lua — `script_register`

Every engine class that wants a Lua face defines a static
`void CClass::script_register(lua_State* L)` in a `*_script.cpp` file, using luabind's DSL:

```cpp
// InventoryOwner_script.cpp
void CInventoryOwner::script_register(lua_State *L)
{
    module(L)
    [
        class_<CInventoryOwner>("CInventoryOwner")
            .def("get_money",      &CInventoryOwner::get_money)
            .def("EnableTrade",    &CInventoryOwner::EnableTrade)
            // ...
    ];
}
```

- `class_<C>("Name")` exposes the class under `Name` in Lua; `.def("lua_name", &C::method)` binds
  a method; `.enum_("...")[ value_("X", X) ]` binds enums; free functions bind via `def(...)`.
- These `script_register` methods are collected and run when the script engine initialises, so
  the names become globally available to gamedata scripts.
- **Surface inventory:** gameplay classes are `xrGame/*_script.cpp`; UI classes are
  `xrGame/ui/*_script.cpp` (UIScriptWnd, UIStatic, UIProgressBar, …). Grep the tree for a method
  name to find its export and the C++ behind it: `rg '"the_lua_name"' src/xrGame`.

### Adding a new exported method (engine devs)

1. Add the C++ method to the class.
2. Add a `.def("lua_name", &CClass::method)` line in that class's `*_script.cpp`.
3. Rebuild. No registration list to touch — the class's `script_register` is already wired.

## 2. Attaching scripts to objects — the binder (`CScriptBinder`)

`CScriptBinder` (`xrGame/script_binder.h/.cpp`) is how a Lua "object" rides a game object's
lifecycle. A bound script receives the engine lifecycle as calls: `reinit`, `reload`, `net_spawn`
/ `net_destroy`, `update(dt)`, `save` / `load`, etc. This is the mechanism behind `bind_*` scripts
in gamedata — the engine instantiates the Lua class named in the object's config and forwards
lifecycle events to it. Look here when a script needs to *persist with* and *tick alongside* a
world object.

## 3. Reacting to engine events — callbacks

Game objects expose a callback registry: Lua registers a function for an engine-defined event id
(death, hit, use, trade, level-border, etc.) and the engine invokes it when the event fires. The
registration entry points are exported on the game-object classes (`xrGame/*_script.cpp`); the
event ids are engine-defined enums. To find the exact set for a build, grep the exports for
`set_callback` / the callback enum in `xrGame`. (Bone/animation callbacks are a separate, lower
level — `stalker_animation_callbacks.cpp`, `animation_script_callback.cpp`.)

## 4. Configuration — LTX and DLTX (the headline modding feature)

Config is `*.ltx` (INI-like: `[section]` + `key = value`, with `:base_section` inheritance).
Scripts read it through `CScriptIniFile` (`xrServerEntities/script_ini_file.h`), a `CInifile`
subclass exposing `section_exist`, `r_string`, `r_float`, … to Lua.

**DLTX** ("Demonized's LTX", `script_ini_file` + the DLTX loader) is this fork's signature modding
capability: mods **override and extend** base LTX sections **without editing base files**, by
shipping `mod_<name>.ltx` fragments that the loader merges at load time. This lets many mods touch
the same config non-destructively — the reason DLTX is called out as a headline feature in
`docs/future-development.md` (MOD-01). The Lua-facing entry includes `DLTX_scriptGetSection`.
Practical effect for modders: add a `mod_*.ltx` with just your deltas rather than copying and
diverging a whole base file.

## 5. Where things live

| Concern | Source |
|---------|--------|
| Per-class exports | `xrGame/*_script.cpp`, `xrGame/ui/*_script.cpp` |
| Lua VM / luabind | `Externals/LuaJIT-git`, luabind (vendored) |
| Object lifecycle binding | `xrGame/script_binder.{h,cpp}`, `script_binder_object*` |
| LTX/DLTX config | `xrServerEntities/script_ini_file.{h,cpp}`, `xrCore` inifile |
| Server entity factory | `CObjectFactory` (server-entity class registration) |

## 6. Caveats

- This document intentionally does **not** enumerate every exported class/method — that list is
  large and moves; the `*_script.cpp` files are the source of truth. Treat this as the map, not
  the territory.
- Exported names are what gamedata scripts see; renaming a `.def("name", …)` is a **breaking
  change** for every mod using it.

See also: `docs/architecture.md`, `docs/future-development.md` (Epic MOD: MOD-01 DLTX,
MOD-04 modder API docs, MOD-05 Lua debugging).
