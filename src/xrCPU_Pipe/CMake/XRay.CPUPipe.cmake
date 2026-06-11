add_module(XRay.CPUPipe
  TYPE STATIC
  
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}

  LINKS
  dxsdk
  fastdelegate
  FastDynamicCast
  loki
  robin_hood

  XRay.Core.Defines
  XRay.Engine.Defines
  
  XRay.Includes
  XRay.Collision.Includes
  XRay.Core.Includes
  XRay.Engine.Includes
  XRay.Render.Common.Includes
  XRay.Render.API.Includes
  XRay.Physics.Includes
  XRay.ServerEntities.Includes
  XRay.Sound.Includes

  DEFINES
  $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0501>
  WIN32_LEAN_AND_MEAN
  RENDER=1

  PRECOMPILES
  #[["windows.h"]]
  #[["stdio.h"]]
  #[["intrin.h"]]

  #[["xrCore.h"]]
  #[["SkeletonXVertRender.h"]]

  #xrCPU_Pipe.h
  #ttapi.h

  SOURCES
  xrCPU_Pipe.cpp
  xrCPU_Pipe.h
)

target_compile_options(XRay.CPUPipe
  PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/wd4005>
)

add_module(XRay.CPUPipe.PLC
  SOURCES
  PLC.cpp
)

add_module(XRay.CPUPipe.Resources
  SOURCES
  resource.h
  xrCPU_Pipe.rc
)

add_module(XRay.CPUPipe.Skinning
  SOURCES
  xrSkin2W.cpp
  xrSkin2W_SSE.cpp        # historical 32-bit asm, wrapped in #if 0
  xrSkin2W_SSE2.cpp       # SSE2 C++ intrinsics (x64)
  xrSkin2W_AVX2.cpp       # AVX2+FMA C++ intrinsics (x64)
  xrSkin2W_thread.cpp
)

# Force /arch:AVX2 on the AVX2 file regardless of build preset.
# Runtime CPUID check in xrCPU_Pipe.cpp ensures these functions only
# run on AVX2-capable hardware even when built in the base preset.
set_source_files_properties(
  ${CMAKE_CURRENT_SOURCE_DIR}/xrSkin2W_AVX2.cpp
  PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>"
)

add_module(XRay.CPUPipe.TTAPI
  SOURCES
  ttapi.cpp
  ttapi.h
)
