# Sprint s2 Design Spec — DX10 Correctness Pass

**Date:** 2026-03-30
**Branch:** `sprint/s2` (worktree: `.worktrees/sprint-2`)
**Scope:** Four correctness bugs in `src/Layers/xrRenderDX10/`. No shader changes. No new files except this spec.

---

## Background

Sprint s1 resolved all P1 visible-artefact bugs and cleaned up the Group H/I TODO noise.
The remaining Group H P2 correctness items were cross-referenced against the post-s1 codebase;
four genuinely open issues were identified:

1. `CRT::create` silently accepts unsupported formats — crashes in `CreateTexture2D` instead of returning gracefully.
2. `CRTC` (cube render target) is entirely unimplemented on DX10/11 — the class and resource manager functions are inside `/* DX10 cut */` blocks; any call path that reaches cube RT creation produces null surfaces.
3. `D3DFMT_A2R10G10B10` is mapped to `DXGI_FORMAT_R10G10B10A2_UNORM` with a `// TODO: gross hack` comment — the channel-order divergence is real and undocumented.
4. The DX10 backbuffer format ignores `ps_r4_hdr10_on` — hardcoded to `R8G8B8A8_UNORM` regardless of the HDR10 setting.

---

## Unit 1 — CRT::create Format Validation

### Problem

`CRT::create` (in `dx10SH_RT.cpp`) has a commented-out `CheckFormatSupport` validation block.
The original code used undefined `D3Dxx_FORMAT_SUPPORT_*` macros. If an unsupported format is
requested, execution falls through to `CreateTexture2D`, which hard-fails.

### Fix

**`DXCommonTypes.h`** — add four `D3D_FORMAT_SUPPORT_*` aliases in both the DX11 section
(mapping to `D3D11_FORMAT_SUPPORT_*`) and the DX10 section (mapping to `D3D10_FORMAT_SUPPORT_*`):

```cpp
#define D3D_FORMAT_SUPPORT_TEXTURE2D      D3D11_FORMAT_SUPPORT_TEXTURE2D      // DX10: D3D10_FORMAT_SUPPORT_TEXTURE2D
#define D3D_FORMAT_SUPPORT_RENDER_TARGET  D3D11_FORMAT_SUPPORT_RENDER_TARGET  // DX10: D3D10_FORMAT_SUPPORT_RENDER_TARGET
#define D3D_FORMAT_SUPPORT_DEPTH_STENCIL  D3D11_FORMAT_SUPPORT_DEPTH_STENCIL  // DX10: D3D10_FORMAT_SUPPORT_DEPTH_STENCIL
#define D3D_FORMAT_SUPPORT_TEXTURECUBE    D3D11_FORMAT_SUPPORT_TEXTURECUBE    // DX10: D3D10_FORMAT_SUPPORT_TEXTURECUBE
```

**`dx10SH_RT.cpp`** — replace the commented-out validation block (lines 84–101) with:

```cpp
// Validate format supports the required usage on this device.
UINT FormatSupport = 0;
if (FAILED(HW.pDevice->CheckFormatSupport(dx10FMT, &FormatSupport))) return;
if (!(FormatSupport & D3D_FORMAT_SUPPORT_TEXTURE2D)) return;
if (!(FormatSupport & (bUseAsDepth ? D3D_FORMAT_SUPPORT_DEPTH_STENCIL : D3D_FORMAT_SUPPORT_RENDER_TARGET))) return;
```

The `//HRESULT _hr;` line at the top of `create()` is not needed (the existing code uses `CHK_DX`
macros, not a local `_hr`). The old commented-out block is removed entirely.

### Commit

`fix(dx10): validate RT format support before CreateTexture2D`

---

## Unit 2 — CRTC DX10/11 Port

### Problem

`CRTC` (cube render target class) is entirely inside `/* DX10 cut */` comment blocks in three files:
`SH_RT.h`, `dx10SH_RT.cpp`, and `dx10ResourceManager_Resources.cpp`. The class definition still
declares D3D9 types (`IDirect3DCubeTexture9*`, `IDirect3DSurface9*[6]`). Any engine path that
calls `_CreateRTC` reaches a CRTC object with all-null surface pointers.

### Fix

**`SH_RT.h`** — uncomment the `CRTC` class block; replace D3D9 types with DX10/11 equivalents:
- `IDirect3DCubeTexture9* pSurface` → `ID3DTexture2D* pSurface`
- `IDirect3DSurface9* pRT[6]` → `ID3DRenderTargetView* pRT[6]`
- Fix `valid()` inversion bug: `return !pTexture` → `return !!pTexture`
- Restore `ref_rtc` typedef

**`dx10SH_RT.cpp`** — replace the `/* DX10 cut */` block with a DX10/11 implementation:

```cpp
CRTC::CRTC()
{
    pSurface = NULL;
    pRT[0] = pRT[1] = pRT[2] = pRT[3] = pRT[4] = pRT[5] = NULL;
    dwSize = 0;
    fmt = D3DFMT_UNKNOWN;
}

CRTC::~CRTC()
{
    destroy();
    DEV->_DeleteRTC(this);
}

void CRTC::create(LPCSTR Name, u32 size, D3DFORMAT f)
{
    if (pSurface) return;
    R_ASSERT(HW.pDevice && Name && Name[0] && size && btwIsPow2(size));
    _order = CPU::GetCLK();
    dwSize = size;
    fmt = f;

    if (size > D3D10_REQ_TEXTURECUBE_DIMENSION) return;

    DXGI_FORMAT dx10FMT = dx10TextureUtils::ConvertTextureFormat(f);

    // Validate format supports cube RT usage on this device.
    UINT FormatSupport = 0;
    if (FAILED(HW.pDevice->CheckFormatSupport(dx10FMT, &FormatSupport))) return;
    if (!(FormatSupport & D3D_FORMAT_SUPPORT_TEXTURECUBE)) return;
    if (!(FormatSupport & D3D_FORMAT_SUPPORT_RENDER_TARGET)) return;

    DEV->Evict();

    D3D_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width            = size;
    desc.Height           = size;
    desc.MipLevels        = 1;
    desc.ArraySize        = 6;
    desc.Format           = dx10FMT;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D_USAGE_DEFAULT;
    desc.BindFlags        = D3D_BIND_SHADER_RESOURCE | D3D_BIND_RENDER_TARGET;
    desc.MiscFlags        = D3D_RESOURCE_MISC_TEXTURECUBE;

    CHK_DX(HW.pDevice->CreateTexture2D(&desc, NULL, &pSurface));
    HW.stats_manager.increment_stats_rtarget(pSurface);

    for (u32 face = 0; face < 6; ++face)
    {
        D3D_RENDER_TARGET_VIEW_DESC rtvDesc;
        ZeroMemory(&rtvDesc, sizeof(rtvDesc));
        rtvDesc.Format                         = dx10FMT;
        rtvDesc.ViewDimension                  = D3D_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice        = 0;
        rtvDesc.Texture2DArray.FirstArraySlice = face;
        rtvDesc.Texture2DArray.ArraySize       = 1;
        CHK_DX(HW.pDevice->CreateRenderTargetView(pSurface, &rtvDesc, &pRT[face]));
    }

    Msg("* created RTc(%s), 6(%d)", Name, size);
    pTexture = DEV->_CreateTexture(Name);
    pTexture->surface_set(pSurface);
}

void CRTC::destroy()
{
    if (!pSurface) return;
    if (pTexture._get())
    {
        pTexture->surface_set(0);
        pTexture.destroy();
        pTexture = nullptr;
    }
    for (u32 face = 0; face < 6; ++face)
        _RELEASE(pRT[face]);
    HW.stats_manager.decrement_stats_rtarget(pSurface);
    _RELEASE(pSurface);
}

void CRTC::reset_begin() { destroy(); }
void CRTC::reset_end()   { create(*cName, dwSize, fmt); }

void resptrcode_crtc::create(LPCSTR Name, u32 size, D3DFORMAT f)
{
    _set(DEV->_CreateRTC(Name, size, f));
}
```

**`dx10ResourceManager_Resources.cpp`** — uncomment `_CreateRTC` and `_DeleteRTC`.

### Notes

- `D3D10_REQ_TEXTURECUBE_DIMENSION` (16384) replaces the undefined `D3Dxx_REQ_TEXTURECUBE_DIMENSION`.
- `stats_manager.increment/decrement_stats_rtarget` mirror the `CRT::create/destroy` pattern.
- The D3D9 `D3DUSAGE_RENDERTARGET`/`D3DPOOL_DEFAULT` parameters are not used in DX10/11.

### Commit

`fix(dx10): port CRTC to DX10/11 — cube render targets were entirely unimplemented`

---

## Unit 3 — A2R10G10B10 Channel-Order Documentation

### Problem

```cpp
{D3DFMT_A2R10G10B10, DXGI_FORMAT_R10G10B10A2_UNORM}, // TODO: gross hack
```

The formats have divergent channel order in their 32-bit word:
- `D3DFMT_A2R10G10B10`: `A[31:30] R[29:20] G[19:10] B[9:0]`
- `DXGI_FORMAT_R10G10B10A2_UNORM`: `R[31:22] G[21:12] B[11:2] A[1:0]`

R and B are swapped. However: `D3DFMT_A2R10G10B10` is only requested in the DX10/11 path as a
hint to select a 10-bit format. No textures with D3D9-authored `A2R10G10B10` memory layout are
loaded through this path — the data is always generated by the DX10/11 pipeline in RGBA order.
The DXGI mapping is functionally correct for this engine's usage even though the naming implies
a different byte ordering. The adjacent `D3DFMT_A2B10G10R10` entry (currently a comment) has
matching channel order and is restored as a proper mapping.

### Fix

**`dx10TextureUtils.cpp`** — replace `// TODO: gross hack` with an accurate comment; restore
`D3DFMT_A2B10G10R10` as an active entry:

```cpp
// D3DFMT_A2R10G10B10 and DXGI_FORMAT_R10G10B10A2_UNORM differ in channel order
// (A2R10G10B10: R[29:20] B[9:0] vs R10G10B10A2: R[31:22] B[11:2]).
// This mapping is correct for DX10/11-generated data (which uses RGBA ordering)
// but would be wrong for D3D9-authored textures. No such textures exist in this path.
{D3DFMT_A2R10G10B10, DXGI_FORMAT_R10G10B10A2_UNORM},
// D3DFMT_A2B10G10R10 has matching channel order to DXGI_FORMAT_R10G10B10A2_UNORM.
{D3DFMT_A2B10G10R10, DXGI_FORMAT_R10G10B10A2_UNORM},
```

### Commit

`fix(dx10): document A2R10G10B10 channel-order divergence; restore A2B10G10R10 mapping`

---

## Unit 4 — DX10 HDR10 Backbuffer Format

### Problem

In `dx10HW.cpp` the DX10 creation path hardcodes the backbuffer format:
```cpp
sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
```
The DX11 path selects `R10G10B10A2_UNORM` when `ps_r4_hdr10_on` is set. The DX10 path ignores
the cvar entirely — HDR10 mode silently degrades to SDR with no warning.

The DX11 colorspace block queries `IDXGISwapChain3` (DXGI 1.4). DX10 hardware predates DXGI 1.4
but modern Windows 10 drivers expose `IDXGISwapChain3` regardless of API version. The DX10
colorspace setup therefore queries the interface, applies it if available, and logs a note if not —
it does not hard-fail.

### Fix

**`dx10HW.cpp` — creation path (~line 371):**
```cpp
#elif defined(USE_DX10)
    sd.BufferDesc.Format = ps_r4_hdr10_on ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
```

**`dx10HW.cpp` — colorspace setup:** the existing `#if defined(USE_DX11)` colorspace block ends
with `#endif`. Change that `#endif` to `#elif defined(USE_DX10)` and add the DX10 block, then
close with `#endif`:
```cpp
#elif defined(USE_DX10)
    if (ps_r4_hdr10_on)
    {
        IDXGISwapChain3* swapchain3 = nullptr;
        if (SUCCEEDED(m_pSwapChain->QueryInterface(&swapchain3)))
        {
            UINT color_space_supported = 0;
            if (SUCCEEDED(swapchain3->CheckColorSpaceSupport(
                    DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &color_space_supported))
                && (color_space_supported & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
            {
                R_CHK(swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020));
            }
            else
            {
                Log("HDR10 color space unsupported on DX10 path, HDR10 output unavailable");
                R_CHK(swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709));
            }
            swapchain3->Release();
        }
        else
        {
            // Note: IDXGISwapChain3 unavailable (DXGI < 1.4). Format is R10G10B10A2_UNORM
            // but colorspace will default to SDR. HDR10 tone mapping will not engage.
            Log("HDR10 requested but IDXGISwapChain3 unavailable on DX10 path");
        }
    }
    else
    {
        IDXGISwapChain3* swapchain3 = nullptr;
        if (SUCCEEDED(m_pSwapChain->QueryInterface(&swapchain3)))
        {
            R_CHK(swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709));
            swapchain3->Release();
        }
    }
#endif
```

The Reset path (`ResizeBuffers` at line 758–764) uses `desc.Format` from `m_ChainDesc.BufferDesc`
which carries the format set at creation time — no change required there.

### Commit

`fix(dx10): respect ps_r4_hdr10_on for backbuffer format and colorspace`

---

## Out of Scope (Sprint s2)

- Triangle fan decomposition — requires call-site audit across the engine; deferred to a dedicated sprint
- HBAO per-subsample — requires shader changes
- Input signature sharing — P3 performance item
- DWM vsync edge case — no reliable fix identified

---

## Post-Implementation Checklist

- [ ] `git log --oneline sprint/s2` shows 4 commits above the branch point
- [ ] `cmake` branch untouched
- [ ] Worktree clean (`git status`)
- [ ] Triage doc Group H table updated to reflect resolved items
