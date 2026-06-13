#pragma once
// dx10DDSLoad — native modern-DDS loader for the DX11 / DX12(11on12) backend.
//
// Bypasses the deprecated D3DX11 image path, which cannot LOD-filter BC7/BC6H
// (the documented reason MS replaced D3DX with DirectXTex). Parses the DDS
// header (legacy FourCC + DXT10 extended header), builds an immutable
// ID3D11Texture2D directly from the file's mip data, and applies the engine's
// top-mip LOD skip by simple offset — no filtering, which is exactly what the
// block-compressed formats need.
//
// Scope (v1): 2D, non-array, non-cube, block-compressed (BC1/2/3/4/5/6H/7).
// Returns true and fills *outTex / *outMips on success. Returns false for
// anything else (cube, array, uncompressed, unrecognised) so the caller falls
// back to the existing D3DX11 path untouched.

#include <d3d11.h>

bool DDSNative_Load2D(
    const void* data, size_t size, int skip_mips,
    ID3D11Device* dev, ID3D11Resource** outTex, unsigned& outMips);
