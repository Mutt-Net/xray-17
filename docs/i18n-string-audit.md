# I18N-03: String Externalisation Audit

**Date:** 2026-06-06  
**Verdict:** System in good shape; three minor hardcoded-string sites found.

---

## How strings work in X-Ray Monolith

All user-facing strings go through `CStringTable` (`src/xrGame/string_table.h`):

```cpp
LPCSTR translated = CStringTable().translate("st_some_key").c_str();
```

Keys are loaded from XML files under `gamedata/configs/text/<lang>/`. The engine picks the active language from `user.ltx`. 150+ call sites in `xrGame` use `translate()` correctly, covering quest text, NPC dialogue, HUD labels, message boxes, and inventory strings.

Message boxes use string-table-keyed text internally:
```cpp
CallMessageBoxOK("not_enough_money_actor");  // resolves via string table at display time
```

---

## Hardcoded strings found

| File | Line | String | Category | Risk |
|------|------|--------|----------|------|
| `UI/UIActorMenuTrade.cpp` | 486 | `"--- RU"` | Fallback placeholder for partner money when no NPC is selected | Low — only shown when partner slot is empty; "RU" = roubles abbreviation |
| `UI/UIEditKeyBind.cpp` | 53, 243 | `"---"` | Unbound-key indicator | Very low — locale-neutral symbol |
| `UI/UIKeyBinding.cpp` | 125 | `"NEXT ITEMS NOT DESCRIBED IN COMMAND DESC LIST"` | Debug/fallback for keybind entries with no description in ltx | Medium — appears in key-binding UI if a command lacks a description entry |
| `UI/UIMapInfo.cpp` | 103 | `"Unknown"` | Fallback for unknown server field in MP browser | Low — SP-irrelevant; MP sessions only |
| `UI/UIOptConCom.cpp` | 70 | `"Stalker"` | Default server name in MP option config | Low — MP only; player can override |

---

## Assessment

No systemic localisation gaps. The `CStringTable` mechanism is properly used throughout all user-visible UI paths. The five sites above are:

- Two MP-only (UIMapInfo, UIOptConCom) — out of scope for the SP-focused fork.
- Two locale-neutral symbols (`"---"`) — no translation needed.
- One actual gap: `"--- RU"` in trade UI — the "RU" suffix (roubles abbreviation) is Russian-specific and could be replaced with a `st_currency_symbol` string-table key to be safe, but this is cosmetic.
- One debug fallback (`"NEXT ITEMS NOT DESCRIBED IN COMMAND DESC LIST"`) — only visible to players who add custom commands without ltx descriptions; trivially fixed by adding descriptions to the ltx if needed.

**Recommended follow-up** (runtime, not code-only):  
Replace `"--- RU"` with `CStringTable().translate("st_roubles_placeholder")` and add the key to all language XMLs. Requires game assets to test.

---

## Pattern to prevent regressions

When adding new UI text: always use `CStringTable().translate("st_<key>")` and add the key to `eng/` and any other active language XML. Never pass a string literal to `SetText()`, `OutText()`, or equivalent unless it is a locale-neutral symbol (`""`, `"---"`, numeric format strings, etc.).
