#pragma once
// R5VK: Vulkan-backend variant of the R5 renderer.
// Shares the entire CRender class with R5/R4; only the CHW implementation
// (vkHW.cpp) differs — Vulkan device owns the swapchain while D3D11
// handles the render pipeline until native Vulkan draw calls replace it.
#include "../xrRenderPC_R4/r4.h"
