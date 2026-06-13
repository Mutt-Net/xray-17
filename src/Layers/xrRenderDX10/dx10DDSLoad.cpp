// dx10DDSLoad.cpp — see header. Self-contained DDS parser + CreateTexture2D.
// Format spec: Microsoft DDS / DDS_HEADER_DXT10 (public). Modelled on the
// DirectXTex DDSTextureLoader logic, trimmed to the 2D block-compressed path.

#include "dx10DDSLoad.h"
#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
constexpr uint32_t kMagic = 0x20534444; // "DDS "

#pragma pack(push, 1)
struct DDS_PIXELFORMAT
{
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t RGBBitCount;
    uint32_t RBitMask, GBitMask, BBitMask, ABitMask;
};
struct DDS_HEADER
{
    uint32_t size;        // 124
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps, caps2, caps3, caps4;
    uint32_t reserved2;
};
struct DDS_HEADER_DXT10
{
    uint32_t dxgiFormat;
    uint32_t resourceDimension; // 3 == TEXTURE2D
    uint32_t miscFlag;          // 0x4 == TEXTURECUBE
    uint32_t arraySize;
    uint32_t miscFlags2;
};
#pragma pack(pop)

constexpr uint32_t DDPF_FOURCC = 0x4;
constexpr uint32_t RES_DIM_TEXTURE2D = 3;
constexpr uint32_t MISC_TEXTURECUBE = 0x4;

inline uint32_t MakeFourCC(char a, char b, char c, char d)
{
    return uint32_t(uint8_t(a)) | (uint32_t(uint8_t(b)) << 8) | (uint32_t(uint8_t(c)) << 16) |
        (uint32_t(uint8_t(d)) << 24);
}

// Legacy FourCC -> DXGI. Returns DXGI_FORMAT_UNKNOWN if not a handled BC FourCC.
DXGI_FORMAT FourCCToDXGI(uint32_t fourCC)
{
    if (fourCC == MakeFourCC('D', 'X', 'T', '1')) return DXGI_FORMAT_BC1_UNORM;
    if (fourCC == MakeFourCC('D', 'X', 'T', '2')) return DXGI_FORMAT_BC2_UNORM;
    if (fourCC == MakeFourCC('D', 'X', 'T', '3')) return DXGI_FORMAT_BC2_UNORM;
    if (fourCC == MakeFourCC('D', 'X', 'T', '4')) return DXGI_FORMAT_BC3_UNORM;
    if (fourCC == MakeFourCC('D', 'X', 'T', '5')) return DXGI_FORMAT_BC3_UNORM;
    if (fourCC == MakeFourCC('B', 'C', '4', 'U')) return DXGI_FORMAT_BC4_UNORM;
    if (fourCC == MakeFourCC('A', 'T', 'I', '1')) return DXGI_FORMAT_BC4_UNORM;
    if (fourCC == MakeFourCC('B', 'C', '4', 'S')) return DXGI_FORMAT_BC4_SNORM;
    if (fourCC == MakeFourCC('B', 'C', '5', 'U')) return DXGI_FORMAT_BC5_UNORM;
    if (fourCC == MakeFourCC('A', 'T', 'I', '2')) return DXGI_FORMAT_BC5_UNORM;
    if (fourCC == MakeFourCC('B', 'C', '5', 'S')) return DXGI_FORMAT_BC5_SNORM;
    return DXGI_FORMAT_UNKNOWN;
}

// Bytes per 4x4 block for the block-compressed formats we accept; 0 = not BC.
uint32_t BlockBytes(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 8;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return 16;
    default:
        return 0;
    }
}

// Surface byte size of one BC mip at (w,h).
size_t BCMipBytes(uint32_t w, uint32_t h, uint32_t blockBytes)
{
    const size_t bw = (w < 1) ? 1 : ((w + 3) / 4);
    const size_t bh = (h < 1) ? 1 : ((h + 3) / 4);
    return bw * bh * blockBytes;
}
} // namespace

bool DDSNative_Load2D(
    const void* data, size_t size, int skip_mips,
    ID3D11Device* dev, ID3D11Resource** outTex, unsigned& outMips)
{
    if (!data || !dev || !outTex) return false;
    if (size < sizeof(uint32_t) + sizeof(DDS_HEADER)) return false;

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    if (*reinterpret_cast<const uint32_t*>(bytes) != kMagic) return false;

    const DDS_HEADER* hdr = reinterpret_cast<const DDS_HEADER*>(bytes + sizeof(uint32_t));
    if (hdr->size != sizeof(DDS_HEADER)) return false;

    size_t dataOffset = sizeof(uint32_t) + sizeof(DDS_HEADER);
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool isCubeOrArray = false;

    const bool hasFourCC = (hdr->ddspf.flags & DDPF_FOURCC) != 0;
    const uint32_t fourCC = hdr->ddspf.fourCC;

    if (hasFourCC && fourCC == MakeFourCC('D', 'X', '1', '0'))
    {
        if (size < dataOffset + sizeof(DDS_HEADER_DXT10)) return false;
        const DDS_HEADER_DXT10* d10 =
            reinterpret_cast<const DDS_HEADER_DXT10*>(bytes + dataOffset);
        dataOffset += sizeof(DDS_HEADER_DXT10);
        format = static_cast<DXGI_FORMAT>(d10->dxgiFormat);
        if (d10->resourceDimension != RES_DIM_TEXTURE2D) return false;
        if (d10->arraySize != 1) isCubeOrArray = true;
        if (d10->miscFlag & MISC_TEXTURECUBE) isCubeOrArray = true;
    }
    else if (hasFourCC)
    {
        format = FourCCToDXGI(fourCC);
    }
    // else: uncompressed RGBA — let D3DX11 handle it (return false below).

    // Cube/array, or anything that isn't a block-compressed 2D surface: fall
    // back to the D3DX11 path.
    const uint32_t blockBytes = BlockBytes(format);
    if (isCubeOrArray || blockBytes == 0) return false;
    if ((hdr->caps2 & 0x200) != 0) return false; // DDSCAPS2_CUBEMAP (legacy)

    uint32_t width = hdr->width;
    uint32_t height = hdr->height;
    uint32_t mipCount = (hdr->mipMapCount > 0) ? hdr->mipMapCount : 1;
    if (width == 0 || height == 0) return false;

    // Clamp the requested top-mip skip so at least one level survives.
    int skip = skip_mips < 0 ? 0 : skip_mips;
    if (skip > int(mipCount) - 1) skip = int(mipCount) - 1;

    // Walk every mip from level 0, tracking file offset; collect the surviving
    // levels (>= skip) into the subresource list. Bounds-checked against size.
    std::vector<D3D11_SUBRESOURCE_DATA> subs;
    subs.reserve(mipCount);
    size_t offset = dataOffset;
    uint32_t w = width, h = height;
    uint32_t topW = width, topH = height;
    for (uint32_t level = 0; level < mipCount; ++level)
    {
        const size_t bw = (w < 1) ? 1 : ((w + 3) / 4);
        const size_t rowBytes = bw * blockBytes;
        const size_t surfBytes = BCMipBytes(w, h, blockBytes);
        if (offset + surfBytes > size) return false; // truncated file — bail to D3DX11

        if (int(level) >= skip)
        {
            if (subs.empty()) { topW = w; topH = h; }
            D3D11_SUBRESOURCE_DATA s;
            s.pSysMem = bytes + offset;
            s.SysMemPitch = static_cast<UINT>(rowBytes);
            s.SysMemSlicePitch = static_cast<UINT>(surfBytes);
            subs.push_back(s);
        }

        offset += surfBytes;
        w = (w > 1) ? (w >> 1) : 1;
        h = (h > 1) ? (h >> 1) : 1;
    }
    if (subs.empty()) return false;

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = topW;
    desc.Height = topH;
    desc.MipLevels = static_cast<UINT>(subs.size());
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = dev->CreateTexture2D(&desc, subs.data(), &tex);
    if (FAILED(hr) || !tex) return false;

    *outTex = tex; // ID3D11Texture2D is an ID3D11Resource
    outMips = static_cast<unsigned>(subs.size());
    return true;
}
