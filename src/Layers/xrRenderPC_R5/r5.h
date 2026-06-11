#pragma once

// R5 is scaffolded on top of the R4 CRender class.
// The DX12 device lives alongside the DX11-compat (11on12) device in CHW;
// all existing render code runs through the 11on12 wrapper unchanged.
// R5-specific behaviour (command lists, ray tracing, mesh shaders) is added
// incrementally in xrRenderDX12/ and xrRenderPC_R5/ without touching shared code.
#include "../xrRenderPC_R4/r4.h"
