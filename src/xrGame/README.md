# xrGame — Game Logic

The largest subsystem (~1 900 files). Contains all game-specific behaviour: the
player actor, AI, inventory, weapons, anomalies, multiplayer, anticheat, and the
Lua script bindings that expose all of the above to mod scripts.

## Responsibilities

| Area | Key files | Notes |
|------|-----------|-------|
| **Actor** | `Actor.cpp/.h`, `ActorCondition*`, `ActorCameras*`, `ActorAnimation*` | Player character: health/stamina, camera, input→animation pipeline, backpack |
| **AI** | `ai/` subdirectory, `AI_PhraseDialogManager.*` | Behaviour trees, path-finding, faction logic, dialog system |
| **Inventory / items** | `Inventory*.cpp`, `Weapon*.cpp`, `Torch*`, `Food*` | Item hierarchy; weapons attach to `CHudItem` for first-person rendering |
| **Anomalies / artifacts** | `Anomaly*.cpp`, `Artifact*.cpp` | Zone hazards and collectibles |
| **Physics** | `physics/`, `PHWorld.cpp`, `IObjectPhysicsCollision.h` | ODE integration; ragdolls, breakable objects, vehicle chassis |
| **Game modes** | `xrServer*.cpp`, `Level*.cpp`, `GamePersistent*.cpp` | Single-player level lifecycle; multiplayer server/client split |
| **Anticheat** | `configs_dump_verifyer.*`, `xrGame/mp_*` | DSA-signed config dump verification (see `docs/security-crypto-audit.md`) |
| **Script bindings** | `*_script.cpp` files throughout | Luabind `module<>` registrations that expose C++ classes to the Lua VM in xrEngine |
| **HUD** | `HUDManager.cpp`, `UIGame*.cpp`, `ui/` | In-world HUD overlays and the full UI stack (menus, inventory screen, PDA) |

## Code conventions

- C++ class `CActor`, `CWeaponAKM`, etc. — `C` prefix is the X-Ray convention for game objects.
- Script-exposed classes have a matching `*_script.cpp` with `luabind::module(L)[class_<…>…]`.
- Physics interactions go through `IObjectPhysicsCollision`; never call ODE directly from game logic.
- All config reads use `pSettings->r_*(section, key)` — section names map to `.ltx` files in `gamedata/config`.

## Lua ↔ C++ surface

The script boundary is at `xrEngine`'s LuaJIT VM. `xrGame` registers its
classes at level-load time. The callable surface is documented in
`docs/modding-api.md`. To add a new binding:

1. Write the C++ implementation in the relevant class.
2. Add a `*_script.cpp` sibling that calls `luabind::module(L)[class_<MyClass>…]`.
3. Document the new function in `docs/modding-api.md`.

## Build

`XRay.Game.Core` static library + `xrGame.dll` that is loaded at runtime by the
engine. Links `XRay.Core`, `XRay.Engine`, `XRay.Physics`, `XRay.ServerEntities`.
The split between static and DLL exists so that the editor tools can link the
static lib without loading the full game DLL.
