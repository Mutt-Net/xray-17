# Sprint s1 Completion Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fully close sprint/s1 by fixing six outstanding items (triage doc, 60Hz windowed bug, screenshot resource leak, alpha ref crash, sun shaft gate, TODO cleanup) and commit each as a discrete change to the `sprint/s1` branch.

**Architecture:** All changes are targeted edits to existing C++ source files and one markdown document. No new files, no shader changes. Each unit is independent and commits cleanly on its own.

**Tech Stack:** C++17, DXGI/DX10/DX11, xray engine render layer (xrRenderDX10, xrRenderPC_R3, xrRenderPC_R4)

**Working directory for ALL operations:** `\\ARDOUGNE\Data\H\Coding\MuttCode\CPP\xray-monolith\.worktrees\sprint-1`

**ALL git commits must be run from that directory.** Commit message bodies must end with `-MuttNET-` on its own line. No `Co-Authored-By` trailers.

---

## Chunk 1: Units 1–3 (doc + two bug fixes)

---

### Task 1: Triage Doc Update (Unit 1)

**Files:**
- Modify: `docs/render-todo-triage.md`

- [ ] **Step 1: Add RESOLVED banners for Groups A, E, G**

  Open `docs/render-todo-triage.md`. Locate the Group A, E, and G section headers.
  Add a resolution line immediately after each header line (matching the pattern used for B, C, D, F).

  **Group A** — find:
  ```markdown
  ## Group A — Half-Pixel Offset (DX10 NDC correction)
  ```
  Change to:
  ```markdown
  ## Group A — Half-Pixel Offset (DX10 NDC correction) ✓ RESOLVED (commit 05b96961)
  ```

  **Group E** — find:
  ```markdown
  ## Group E — HDR Texture Format Incompatibility (DX11)
  ```
  Change to:
  ```markdown
  ## Group E — HDR Texture Format Incompatibility (DX11) ✓ RESOLVED (E2-T5, commit 527180c6)
  ```
  Add a resolution note below the existing description:
  ```markdown
  `t_ss_async` staging texture now queries `DXGI_SWAP_CHAIN_DESC` at creation and
  uses `scDesc.BufferDesc.Format`, matching the swapchain format in both SDR and HDR
  modes. `CopyResource` format mismatch eliminated.
  ```

  **Group G** — find:
  ```markdown
  ## Group G — Blender Missing Implementations
  ```
  Change to:
  ```markdown
  ## Group G — Blender Missing Implementations ✓ RESOLVED (commit 3a92cc8a)
  ```
  Add resolution note:
  ```markdown
  All blender `Compile()` methods (both base and `_msaa` variants) were already fully
  implemented with `r_dx10Texture` / `r_dx10Sampler` calls. The TODO markers predated
  the DX10 implementations and were stale across all six blenders. Removed in sprint/s1.
  ```

- [ ] **Step 2: Update Groups H and I status**

  **Group H** — find the section header and update it:
  ```markdown
  ## Group H — xrRenderDX10 Device Layer
  ```
  Change to:
  ```markdown
  ## Group H — xrRenderDX10 Device Layer ✓ CLEANUP DONE (commit 3d8bc3fe)
  ```
  Add note before the table:
  ```markdown
  Stale TODO noise removed in sprint/s1. Remaining items below are real open issues
  documented as knowledge comments in the source. No further sprint/s1 action.
  ```

  **Group I** — find:
  ```markdown
  ## Group I — Performance / Miscellaneous
  ```
  Change to:
  ```markdown
  ## Group I — Performance / Miscellaneous ✓ CLEANUP DONE (sprint/s1)
  ```
  Add note:
  ```markdown
  All TODO markers converted to knowledge comments in sprint/s1. Items remain valid
  future work but are not blocking.
  ```

- [ ] **Step 3: Update the Summary table**

  Find the Summary table at the bottom of the file. Update the Action column for rows
  that have changed:

  | Priority | Count | Action |
  |----------|-------|--------|
  | P1 | ~18 | ✓ Resolved: half-pixel offset (A), rain stale (B), HDR copy (E). |
  | P2 | ~60 | ✓ Resolved: shadow mapping (C,D), stencil (F), blenders (G). Open: H items. |
  | P3 | ~20 | Documented as knowledge comments (H, I). |

- [ ] **Step 4: Commit**

  ```bash
  cd "\\ARDOUGNE\Data\H\Coding\MuttCode\CPP\xray-monolith\.worktrees\sprint-1"
  git add docs/render-todo-triage.md
  git commit -m "docs(triage): mark Groups A, E, G, H, I resolved or closed

  All sprint/s1 work reflected:
  - A: 14 half-pixel offset sites fixed (05b96961)
  - E: t_ss_async format matched to swapchain (527180c6)
  - G: stale blender TODOs removed; all six blenders were already fully implemented (3a92cc8a)
  - H: noise cleaned (3d8bc3fe); real issues documented as knowledge comments
  - I: TODO markers converted to knowledge comments

  -MuttNET-"
  ```

---

### Task 2: 60 Hz Windowed Refresh Rate (Unit 2)

**Files:**
- Modify: `src/Layers/xrRenderDX10/dx10HW.cpp`

Two sites — initial swapchain creation path (~line 410) and device reset path (~line 707).
In windowed mode, DXGI ignores the refresh rate but `{0,0}` is the documented neutral
value. Hardcoding 60 triggers validation warnings on non-60Hz displays.

- [ ] **Step 1: Fix site 1 — initial swapchain creation**

  Find the block (around line 408–417):
  ```cpp
  if (bWindowed) {
      // TODO: fix this, shouldn't just default to 60hz
  #if defined(USE_DX11)
      sd_fullscreen.RefreshRate.Numerator   = 60;
      sd_fullscreen.RefreshRate.Denominator = 1;
  #elif defined(USE_DX10)
      sd.BufferDesc.RefreshRate.Numerator = 60;
      sd.BufferDesc.RefreshRate.Denominator = 1;
  #endif
  }
  ```

  Replace with (use tabs to match the surrounding file — the lines inside the `#if` blocks
  are tab-indented in the original):
  ```cpp
  if (bWindowed) {
	// Windowed mode: DXGI ignores RefreshRate; {0,0} is the neutral value.
  #if defined(USE_DX11)
	sd_fullscreen.RefreshRate.Numerator   = 0;
	sd_fullscreen.RefreshRate.Denominator = 0;
  #elif defined(USE_DX10)
	sd.BufferDesc.RefreshRate.Numerator = 0;
	sd.BufferDesc.RefreshRate.Denominator = 0;
  #endif
  }
  ```

- [ ] **Step 2: Fix site 2 — device reset path**

  Find the block (around line 705–715):
  ```cpp
  if (bWindowed)
  {
  #if defined(USE_DX11)
      // TODO: fix
      cd_fs.RefreshRate.Numerator = 60;
      cd_fs.RefreshRate.Denominator = 1;
  #elif defined(USE_DX10)
      desc.RefreshRate.Numerator = 60;
      desc.RefreshRate.Denominator = 1;
  #endif
  }
  ```

  Replace with (tab-indented to match the surrounding file):
  ```cpp
  if (bWindowed)
  {
	// Windowed mode: DXGI ignores RefreshRate; {0,0} is the neutral value.
  #if defined(USE_DX11)
	cd_fs.RefreshRate.Numerator = 0;
	cd_fs.RefreshRate.Denominator = 0;
  #elif defined(USE_DX10)
	desc.RefreshRate.Numerator = 0;
	desc.RefreshRate.Denominator = 0;
  #endif
  }
  ```

- [ ] **Step 3: Verify diff**

  ```bash
  cd "\\ARDOUGNE\Data\H\Coding\MuttCode\CPP\xray-monolith\.worktrees\sprint-1"
  git diff src/Layers/xrRenderDX10/dx10HW.cpp
  ```

  Expected: 10 lines changed — two `TODO` comment lines removed, and eight value lines
  changed (`= 60` → `= 0` and `= 1` → `= 0` for both Numerator and Denominator in each
  of the two DX10 and DX11 sub-branches, across both sites).
  No fullscreen paths touched.

- [ ] **Step 4: Commit**

  ```bash
  git add src/Layers/xrRenderDX10/dx10HW.cpp
  git commit -m "fix(dx10): use DXGI {0,0} for windowed refresh rate

  DXGI ignores the refresh rate field for windowed swapchains.
  {0,0} is the documented neutral value; {60,1} was hardcoded and
  produces validation warnings on displays not running at 60 Hz.
  Both creation and reset paths updated (DX10 + DX11 branches).

  -MuttNET-"
  ```

---

### Task 3: DoAsyncScreenshot Resource Leak (Unit 3)

**Files:**
- Modify: `src/Layers/xrRenderPC_R4/r4_rendertarget_phase_combine.cpp`
- Modify: `src/Layers/xrRenderPC_R3/r3_rendertarget_phase_combine.cpp`

Three defects (R4 has all three; R3 has two — `__uuidof` is already correct in R3):
1. `pBuffer` COM reference leaked after every screenshot
2. `hr` from `GetBuffer` never checked — null dereference crash on failure
3. (R4 only) `__uuidof(ID3D10Texture2D)` wrong on DX11 device — should be `__uuidof(ID3DTexture2D)`

Also: stale `// TODO: fox that later` comment in both files. Note: the original HACK
comment in both files also contains typos (`CopyResourcess`, `targetr`) — the
replacement bodies below correct them incidentally.

- [ ] **Step 1: Fix R4**

  In `r4_rendertarget_phase_combine.cpp`, find `DoAsyncScreenshot` (top of file).

  Replace the entire function body with (note: `Igor:` attribution removed from the
  comment — it was an author tag, not useful context):
  ```cpp
  void CRenderTarget::DoAsyncScreenshot()
  {
	//	Screenshot will not have postprocess applied.
	if (RImplementation.m_bMakeAsyncSS)
	{
		HRESULT hr;

		//	HACK: unbind RT. CopyResource needs src and target to be unbound.
		//u_setrt				( Device.dwWidth,Device.dwHeight,HW.pBaseRT,NULL,NULL,HW.pBaseZB);

		ID3DTexture2D* pBuffer;
		hr = HW.m_pSwapChain->GetBuffer(0, __uuidof(ID3DTexture2D), (LPVOID*)&pBuffer);
		VERIFY(SUCCEEDED(hr));
		HW.pContext->CopyResource(t_ss_async, pBuffer);
		pBuffer->Release();

		RImplementation.m_bMakeAsyncSS = false;
	}
  }
  ```

  Key changes vs. original:
  - `__uuidof(ID3D10Texture2D)` → `__uuidof(ID3DTexture2D)` (correct GUID for DX11 device)
  - `VERIFY(SUCCEEDED(hr))` added after `GetBuffer`
  - `pBuffer->Release()` added after `CopyResource`
  - `// TODO: fox that later` removed
  - Commented-out dead code blocks (`ID3DTexture2D *pTex = 0; ...`) removed for clarity

- [ ] **Step 2: Fix R3**

  In `r3_rendertarget_phase_combine.cpp`, find `DoAsyncScreenshot` (top of file).

  The R3 version uses `ID3D10Texture2D* pBuffer` and `__uuidof(ID3D10Texture2D)` — this
  is correct for DX10 (`ID3DTexture2D` = `ID3D10Texture2D` under DX10). Do NOT change
  the `__uuidof`. Apply only the hr check and Release:

  ```cpp
  void CRenderTarget::DoAsyncScreenshot()
  {
	//	Screenshot will not have postprocess applied.
	if (RImplementation.m_bMakeAsyncSS)
	{
		HRESULT hr;

		//	HACK: unbind RT. CopyResource needs src and target to be unbound.
		//u_setrt				( Device.dwWidth,Device.dwHeight,HW.pBaseRT,NULL,NULL,HW.pBaseZB);

		ID3D10Texture2D* pBuffer;
		hr = HW.m_pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (LPVOID*)&pBuffer);
		VERIFY(SUCCEEDED(hr));
		HW.pDevice->CopyResource(t_ss_async, pBuffer);
		pBuffer->Release();

		RImplementation.m_bMakeAsyncSS = false;
	}
  }
  ```

  Key changes vs. original:
  - `VERIFY(SUCCEEDED(hr))` added
  - `pBuffer->Release()` added
  - `// TODO: fox that later` removed
  - Dead commented-out code blocks removed

- [ ] **Step 3: Verify diff**

  ```bash
  cd "\\ARDOUGNE\Data\H\Coding\MuttCode\CPP\xray-monolith\.worktrees\sprint-1"
  git diff src/Layers/xrRenderPC_R4/r4_rendertarget_phase_combine.cpp \
           src/Layers/xrRenderPC_R3/r3_rendertarget_phase_combine.cpp
  ```

  Expected for R4: `__uuidof` changed, `VERIFY` added, `Release` added, `TODO` removed, dead commented blocks removed.
  Expected for R3: same minus the `__uuidof` change.

- [ ] **Step 4: Commit**

  ```bash
  git add src/Layers/xrRenderPC_R4/r4_rendertarget_phase_combine.cpp \
          src/Layers/xrRenderPC_R3/r3_rendertarget_phase_combine.cpp
  git commit -m "fix(r3,r4): release pBuffer and check hr in DoAsyncScreenshot

  GetBuffer increments the COM refcount on pBuffer but Release() was
  never called — one texture reference leaked per screenshot.
  hr was assigned but never checked; a failed GetBuffer would leave
  pBuffer null and crash CopyResource.

  R4 (DX11): also fixes __uuidof(ID3D10Texture2D) -> __uuidof(ID3DTexture2D).
  Under DX11, ID3DTexture2D is ID3D11Texture2D; the DX10 GUID returned
  E_NOINTERFACE on a DX11 swapchain.

  R3 (DX10): __uuidof(ID3D10Texture2D) is correct and unchanged.

  -MuttNET-"
  ```

---

## Chunk 2: Units 4–6 (alpha ref + sun fix + comment pass)

---

### Task 4: Alpha Ref No-Op (Unit 4)

**Files:**
- Modify: `src/Layers/xrRenderDX10/dx10R_Backend_Runtime.h`

`CBackend::set_AlphaRef` crashes with `VERIFY(!"Not implemented.")`. DX10+ has no
hardware alpha-ref state — alpha testing is done in shader via `clip()`. The only
caller is `dxUIRender::SetAlphaRef`, which is a virtual interface method with no
call sites in xrGame/xrEngine. The DX10 alpha-ref functionality that does work is
`dx10StateManager::SetAlphaRef` → shader constant (a completely separate path,
unaffected by this change).

- [ ] **Step 1: Replace the VERIFY crash with a documented no-op**

  Replace the entire `set_AlphaRef` function body using two edits to avoid
  a trailing-space match issue on one of the commented lines.

  **Edit A** — remove the TODO, VERIFY, and stale comment (unique portion that avoids
  the `//{ ` trailing-space line). Find:
  ```cpp
	//	TODO: DX10: Implement rasterizer state update to support alpha ref
	VERIFY(!"Not implemented.");
  ```
  Replace with the new documented no-op comment and `(void)_value;`:
  ```cpp
	// DX10+: no hardware alpha-ref state. Alpha test is performed in shader
	// via clip(). Alpha ref for shader-driven state (dx10StateManager) is
	// handled separately via BindAlphaRef / set_c. This path (via
	// dxUIRender::SetAlphaRef) has no active callers in DX10/11 paths.
	(void)_value;
  ```

  **Edit B** — remove the remaining commented-out DX9 block (3 lines). Find:
  ```cpp
	//if (alpha_ref != _value)
  ```
  And delete through the closing `//}` — these 4 lines (`//if`, `//{ `, `//\t...`, `//}`)
  should be removed entirely. Read the file first to get the exact bytes including the
  trailing space on the `//{ ` line before constructing the old_string.

  After both edits the function should be:
  ```cpp
  IC void CBackend::set_AlphaRef(u32 _value)
  {
	// DX10+: no hardware alpha-ref state. Alpha test is performed in shader
	// via clip(). Alpha ref for shader-driven state (dx10StateManager) is
	// handled separately via BindAlphaRef / set_c. This path (via
	// dxUIRender::SetAlphaRef) has no active callers in DX10/11 paths.
	(void)_value;
  }
  ```

  Verify with:
  ```cpp
  IC void CBackend::set_AlphaRef(u32 _value)
  {
	// DX10+: no hardware alpha-ref state. Alpha test is performed in shader
	// via clip(). Alpha ref for shader-driven state (dx10StateManager) is
	// handled separately via BindAlphaRef / set_c. This path (via
	// dxUIRender::SetAlphaRef) has no active callers in DX10/11 paths.
	(void)_value;
  }
  ```

- [ ] **Step 2: Verify diff**

  ```bash
  cd "\\ARDOUGNE\Data\H\Coding\MuttCode\CPP\xray-monolith\.worktrees\sprint-1"
  git diff src/Layers/xrRenderDX10/dx10R_Backend_Runtime.h
  ```

  Expected: VERIFY crash line removed, commented-out DX9 block removed, comment + `(void)_value` added.

- [ ] **Step 3: Commit**

  ```bash
  git add src/Layers/xrRenderDX10/dx10R_Backend_Runtime.h
  git commit -m "chore(dx10): replace set_AlphaRef crash with intentional no-op

  DX10+ has no D3DRS_ALPHAREF hardware state; alpha test is done in
  shader via clip(). The only caller (dxUIRender::SetAlphaRef) has no
  active call sites in DX10/11 paths. The working DX10 alpha-ref
  implementation (dx10StateManager::SetAlphaRef -> shader constant)
  is a separate path and is unaffected.

  -MuttNET-"
  ```

---

### Task 5: Sun Color Gate on Shaft Rendering (Unit 5)

**Files:**
- Modify: `src/Layers/xrRenderPC_R3/r3_rendertarget.cpp`
- Modify: `src/Layers/xrRenderPC_R4/r4_rendertarget.cpp`

`need_to_render_sunshafts()` checks `m_fSunShaftsIntensity` but not `sun_color`.
A fully black sun passes the gate and runs shaft rendering unnecessarily.
`CEnvDescriptor::sun_color` is a `Fvector3` (R,G,B). `square_magnitude()` is an
existing method on `Fvector3` — no new headers required.

Both R3 and R4 have identical copies of this function. The change is a one-line
insertion + removal of the stale TODO comment between the declarations.

- [ ] **Step 1: Fix R3**

  In `r3_rendertarget.cpp`, find `need_to_render_sunshafts()`. Locate this block
  (tab-indented — one tab for `{`, two tabs for body lines):
  ```cpp
	{
		CEnvDescriptor& E = *g_pGamePersistent->Environment().CurrentEnv;
		float fValue = E.m_fSunShaftsIntensity;
		//	TODO: add multiplication by sun color here
		if (fValue < 0.0001) return false;
	}
  ```

  Replace with:
  ```cpp
	{
		CEnvDescriptor& E = *g_pGamePersistent->Environment().CurrentEnv;
		float fValue = E.m_fSunShaftsIntensity;
		if (fValue < 0.0001) return false;
		if (E.sun_color.square_magnitude() < 0.0001f) return false;
	}
  ```

  Changes: TODO comment removed, `sun_color` check inserted after intensity check.

- [ ] **Step 2: Fix R4**

  In `r4_rendertarget.cpp`, find the same block and apply the identical change.

- [ ] **Step 3: Verify diff**

  ```bash
  cd "\\ARDOUGNE\Data\H\Coding\MuttCode\CPP\xray-monolith\.worktrees\sprint-1"
  git diff src/Layers/xrRenderPC_R3/r3_rendertarget.cpp \
           src/Layers/xrRenderPC_R4/r4_rendertarget.cpp
  ```

  Expected: each file has one line removed (the TODO comment) and one line added (the `sun_color` check). No other changes.

- [ ] **Step 4: Commit**

  ```bash
  git add src/Layers/xrRenderPC_R3/r3_rendertarget.cpp \
          src/Layers/xrRenderPC_R4/r4_rendertarget.cpp
  git commit -m "fix(r3,r4): gate sun shaft rendering on sun color luminance

  need_to_render_sunshafts() checked m_fSunShaftsIntensity but not
  sun_color. A fully black sun (sun_color = {0,0,0}) would still pass
  the gate and trigger shaft rendering. The effective contribution is
  intensity * sun_color — both must be non-negligible.

  -MuttNET-"
  ```

---

### Task 6: Investigation TODO → Knowledge Comment Pass (Unit 6)

**Files:**
- Modify: `src/Layers/xrRenderDX10/dx10HW.cpp`
- Modify: `src/Layers/xrRenderDX10/dx10R_Backend_Runtime.h`
- Modify: `src/Layers/xrRenderPC_R4/r4.cpp`
- Modify: `src/Layers/xrRenderPC_R3/r3.cpp`
- Modify: `src/Layers/xrRenderPC_R4/light_vis.cpp`
- Modify: `src/Layers/xrRenderPC_R3/light_vis.cpp`
- Modify: `src/Layers/xrRenderPC_R4/r4_rendertarget_phase_combine.cpp`
- Modify: `src/Layers/xrRenderPC_R4/r4_rendertarget_phase_accumulator.cpp`
- Modify: `src/Layers/xrRenderPC_R3/r3_rendertarget_phase_accumulator.cpp`

This task has two distinct parts: (A) convert plain TODO comments to knowledge comments,
and (B) fix the MSAA accumulator depth binding.

#### Part A — Comment Conversions

- [ ] **Step 1: dx10HW.cpp — DWM vsync TODO**

  Find (~line 147) — note 4-space indentation:
  ```cpp
      // TODO: On some PC configurations (versions of Windows, graphics drivers, currently unknown what exactly) this isn't
  ```
  Change `TODO:` to `Note:`:
  ```cpp
      // Note: On some PC configurations (versions of Windows, graphics drivers, currently unknown what exactly) this isn't
  ```

- [ ] **Step 2: dx10HW.cpp — SDR 10-bit TODO**

  Find (~line 482):
  ```cpp
      // TODO: SDR 10-bit?
  ```
  Remove the line entirely.

- [ ] **Step 3: dx10R_Backend_Runtime.h — shader statistics TODOs**

  Four occurrences to replace (~lines 84, 105, 121, 137). The G and H lines are unique;
  the D line appears twice (DS and CS shaders — identical text). Handle as three Edit calls:

  All three sub-steps use 2-tab indentation before `//` (matching the file).

  **3a** — Replace unique G line:
  ```cpp
		//	TODO: DX10: Get statistics for G Shader change
  ```
  Replace with:
  ```cpp
		//	DX10: shader change statistics not wired up (ID3D10Query per-draw; not required for correctness).
  ```

  **3b** — Replace unique H line:
  ```cpp
		//	TODO: DX10: Get statistics for H Shader change
  ```
  Replace with:
  ```cpp
		//	DX10: shader change statistics not wired up (ID3D10Query per-draw; not required for correctness).
  ```

  **3c** — Replace both identical D lines using `replace_all: true` (covers DS at ~line 121 and CS at ~line 137):
  ```cpp
		//	TODO: DX10: Get statistics for D Shader change
  ```
  Replace with (`replace_all: true`):
  ```cpp
		//	DX10: shader change statistics not wired up (ID3D10Query per-draw; not required for correctness).
  ```

- [ ] **Step 4: dx10R_Backend_Runtime.h — triangle fan TODO**

  Find (~line 331):
  ```cpp
	//	TODO: DX10: Remove triangle fan usage from the engine
	if (T == D3DPT_TRIANGLEFAN)
		return;
  ```
  Replace with:
  ```cpp
	//	DX10: triangle fan topology is unsupported. Early return is intentional;
	//	callers using D3DPT_TRIANGLEFAN are legacy DX9 paths. Decomposition to
	//	triangle lists would require an audit of all call sites.
	if (T == D3DPT_TRIANGLEFAN)
		return;
  ```

- [ ] **Step 5: r4.cpp — input signature sharing TODO**

  Find (~line 1109) — three tabs before `//`:
  ```cpp
			//	TODO: DX10: share the same input signatures
  ```
  Replace with:
  ```cpp
			//	DX10: input signatures could be shared across VS variants to reduce
			//	InputLayout creation overhead. Valid optimisation; not a correctness issue.
  ```

- [ ] **Step 6: r3.cpp — input signature sharing TODO**

  Find (~line 996) the same string with the same 3-tab indentation and apply
  the identical replacement as Step 5.

- [ ] **Step 7: r4.cpp — HBAO TODO**

  Find (~line 387):
  ```cpp
	//	TODO: fix hbao shader to allow to perform per-subsample effect!
  ```
  Replace with:
  ```cpp
	//	HBAO per-subsample effect requires shader changes; forced off for now.
  ```

- [ ] **Step 8: r3.cpp — HBAO TODO**

  Find (~line 363) the same HBAO TODO and apply the same replacement.

- [ ] **Step 9: light_vis.cpp R4 — sort TODO**

  Find (~line 57) in `src/Layers/xrRenderPC_R4/light_vis.cpp`:
  ```cpp
	//	TODO: sort for performance improvement if this technique hurts
  ```
  Replace with:
  ```cpp
	//	Sorting volumetric lights for performance is a valid P3 optimisation.
  ```

- [ ] **Step 10: light_vis.cpp R3 — sort TODO**

  Find (~line 54) in `src/Layers/xrRenderPC_R3/light_vis.cpp` and apply the same replacement.

- [ ] **Step 11: r4_rendertarget_phase_combine.cpp — MSAA+bloom copy TODO**

  Find (~line 716):
  ```cpp
		// TODO: we should be able to avoid a copy if both are enabled
  ```
  Replace with:
  ```cpp
		// A copy could be avoided when both MSAA and HDR bloom are enabled; minor optimisation.
  ```

#### Part B — MSAA Accumulator Depth Fix

- [ ] **Step 12: Fix R4 phase_vol_accumulator**

  In `src/Layers/xrRenderPC_R4/r4_rendertarget_phase_accumulator.cpp`,
  locate `phase_vol_accumulator()`. The non-SSfx else-branch starts with a 15-line
  TODO comment block. The full old_string must include that block plus the two
  commented-out DX9 lines inside the `if (!m_bHasActiveVolumetric)` sub-block:
  ```cpp
	else
	{
		// TODO: there is a bug in this function when MSAA is enabled, if you run with the D3D debug layer enabled, you'll get this error:
		// ID3D11DeviceContext::OMSetRenderTargets: The RenderTargetView at slot 0 is not compatible with the DepthStencilView. DepthStencilViews may only be used with RenderTargetViews if the effective dimensions of the Views are equal, as well as the Resource types, multisample count, and multisample quality. The RenderTargetView at slot 0 has (w:2560,h:1440,as:1), while the Resource is a Texture2D with (mc:4,mq:4294967295). The DepthStencilView has (w:2560,h:1440,as:1), while the Resource is a Texture2D with (mc:1,mq:0).
		// this seems to be because the pBaseZB target is always (SampleCount, SampleQuality) = (1, 0) (see dx10HW.cpp) but the
		// `rt_Generic_2` render target is a multisampled rendertarget
		// Not sure how to fix it correctly, if it doesn't need a depth buffer then just pass NULL, if it does
		// we probably need to create a multisampled depth target (e.g. `rt_Generic_2_zb`)
		//
		// The best way I've found to see this happen is to launch the DX11 exe with `--dxgi-dbg` to enable the debug layer,
		// then launch the game in RenderDoc with `Enable API Validation` and `Collect Callstacks` enabled, take a capture
		// then go to `Tools > Load Symbols` to load the callstack symbols. In the bottom left of the main RenderDoc window
		// there's a little status message that you can click on to see all the debug layer errors/warnings that lets you jump
		// directly to the render event where the error occurred.
		//
		// This will let you go to the precise render event that caused the debug layer to log an error, and let you see the
		// callstack where the Render() call occurred
		if (!m_bHasActiveVolumetric)
		{
			m_bHasActiveVolumetric = true;
			if (!RImplementation.o.dx10_msaa)
				u_setrt(rt_Generic_2, NULL, NULL, HW.pBaseZB);
			else
				u_setrt(rt_Generic_2, NULL, NULL, RImplementation.Target->rt_MSAADepth->pZRT);
			//u32		clr4clearVol				= color_rgba(0,0,0,0);	// 0x00
			//CHK_DX	(HW.pDevice->Clear			( 0L, NULL, D3DCLEAR_TARGET, clr4clearVol, 1.0f, 0L));
			FLOAT ColorRGBA[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			HW.pContext->ClearRenderTargetView(rt_Generic_2->pRT, ColorRGBA);
		}
		else
		{
			if (!RImplementation.o.dx10_msaa)
				u_setrt(rt_Generic_2, NULL, NULL, HW.pBaseZB);
			else
				u_setrt(rt_Generic_2, NULL, NULL, RImplementation.Target->rt_MSAADepth->pZRT);
		}
	}
  ```

  Replace with:
  ```cpp
	else
	{
		// Volumetric accumulation is additive blending — no depth write or clip required.
		// Passing NULL for depth mirrors the SSfx path and eliminates the D3D debug layer
		// error: RT sample count must match depth-stencil sample count.
		if (!m_bHasActiveVolumetric)
		{
			m_bHasActiveVolumetric = true;
			u_setrt(rt_Generic_2, NULL, NULL, NULL);
			FLOAT ColorRGBA[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			HW.pContext->ClearRenderTargetView(rt_Generic_2->pRT, ColorRGBA);
		}
		else
		{
			u_setrt(rt_Generic_2, NULL, NULL, NULL);
		}
	}
  ```

- [ ] **Step 13: Fix R3 phase_vol_accumulator**

  In `src/Layers/xrRenderPC_R3/r3_rendertarget_phase_accumulator.cpp`,
  `phase_vol_accumulator()` has no SSfx guard — the entire function body is the
  equivalent of R4's non-SSfx path. Find and replace all four `u_setrt` calls:

  ```cpp
	if (!m_bHasActiveVolumetric)
	{
		m_bHasActiveVolumetric = true;
		if (!RImplementation.o.dx10_msaa)
			u_setrt(rt_Generic_2, NULL,NULL, HW.pBaseZB);
		else
			u_setrt(rt_Generic_2, NULL,NULL, RImplementation.Target->rt_MSAADepth->pZRT);
		//u32		clr4clearVol				= color_rgba(0,0,0,0);	// 0x00
		//CHK_DX	(HW.pDevice->Clear			( 0L, NULL, D3DCLEAR_TARGET, clr4clearVol, 1.0f, 0L));
		FLOAT ColorRGBA[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		HW.pDevice->ClearRenderTargetView(rt_Generic_2->pRT, ColorRGBA);
	}
	else
	{
		if (!RImplementation.o.dx10_msaa)
			u_setrt(rt_Generic_2, NULL,NULL, HW.pBaseZB);
		else
			u_setrt(rt_Generic_2, NULL,NULL, RImplementation.Target->rt_MSAADepth->pZRT);
	}
  ```

  Replace with:
  ```cpp
	// Volumetric accumulation is additive blending — no depth write or clip required.
	// Passing NULL for depth eliminates the D3D debug layer error when MSAA is active:
	// RT sample count must match depth-stencil sample count.
	if (!m_bHasActiveVolumetric)
	{
		m_bHasActiveVolumetric = true;
		u_setrt(rt_Generic_2, NULL,NULL, NULL);
		FLOAT ColorRGBA[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		HW.pDevice->ClearRenderTargetView(rt_Generic_2->pRT, ColorRGBA);
	}
	else
	{
		u_setrt(rt_Generic_2, NULL,NULL, NULL);
	}
  ```

- [ ] **Step 14: Verify diff**

  ```bash
  cd "\\ARDOUGNE\Data\H\Coding\MuttCode\CPP\xray-monolith\.worktrees\sprint-1"
  git diff src/Layers/xrRenderDX10/dx10HW.cpp \
           src/Layers/xrRenderDX10/dx10R_Backend_Runtime.h \
           src/Layers/xrRenderPC_R4/r4.cpp \
           src/Layers/xrRenderPC_R3/r3.cpp \
           src/Layers/xrRenderPC_R4/light_vis.cpp \
           src/Layers/xrRenderPC_R3/light_vis.cpp \
           src/Layers/xrRenderPC_R4/r4_rendertarget_phase_combine.cpp \
           src/Layers/xrRenderPC_R4/r4_rendertarget_phase_accumulator.cpp \
           src/Layers/xrRenderPC_R3/r3_rendertarget_phase_accumulator.cpp
  ```

  Expected: only comment text changes plus the MSAA accumulator `u_setrt` depth args
  changed from `HW.pBaseZB` / `rt_MSAADepth->pZRT` to `NULL`. No logic changes anywhere
  except the accumulator files.

- [ ] **Step 15: Commit**

  ```bash
  git add src/Layers/xrRenderDX10/dx10HW.cpp \
          src/Layers/xrRenderDX10/dx10R_Backend_Runtime.h \
          src/Layers/xrRenderPC_R4/r4.cpp \
          src/Layers/xrRenderPC_R3/r3.cpp \
          src/Layers/xrRenderPC_R4/light_vis.cpp \
          src/Layers/xrRenderPC_R3/light_vis.cpp \
          src/Layers/xrRenderPC_R4/r4_rendertarget_phase_combine.cpp \
          src/Layers/xrRenderPC_R4/r4_rendertarget_phase_accumulator.cpp \
          src/Layers/xrRenderPC_R3/r3_rendertarget_phase_accumulator.cpp
  git commit -m "chore(render): convert investigation TODOs to knowledge comments

  All remaining TODO/FIXME markers that described known limitations
  without actionable fixes converted to explanatory comments:
  - dx10HW: DWM vsync edge case (prefix only), SDR 10-bit removed
  - dx10R_Backend_Runtime: shader stats, triangle fan
  - r3/r4: input signature sharing, HBAO per-subsample
  - light_vis (R3+R4): volumetric light sort
  - r4_rendertarget_phase_combine: MSAA+bloom copy avoidance

  Also fixes MSAA accumulator depth binding (R3+R4):
  phase_vol_accumulator() is additive blending — no depth needed.
  Passing NULL for depth eliminates the D3D debug layer error
  (RT/DSV sample count mismatch). Mirrors existing SSfx path (R4).

  -MuttNET-"
  ```

---

## Post-Implementation Checklist

- [ ] Run `git log --oneline sprint/s1` and confirm 6 new commits above `3d8bc3fe`
- [ ] Verify `cmake` branch is untouched: `git log --oneline cmake | head -3`
- [ ] Confirm worktree is clean: `git status` from `.worktrees/sprint-1`
- [ ] Sprint ready to merge to `cmake` once visual regression tested against DX9 reference
