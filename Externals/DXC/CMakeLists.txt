# DXC — DirectX Shader Compiler (HLSL → SPIR-V)
# Sourced from the LunarG Vulkan SDK already required by the R5VK targets.
# No separate download needed.

if(NOT Vulkan_FOUND)
  message(STATUS "DXC: Vulkan SDK not found, DXC unavailable")
  return()
endif()

# Resolve SDK root from the known Vulkan include dir
get_filename_component(VK_SDK_ROOT "${Vulkan_INCLUDE_DIR}/.." ABSOLUTE)

set(DXC_INCLUDE_DIR "${VK_SDK_ROOT}/Include/dxc" CACHE PATH "DXC header directory")
set(DXC_LIB        "${VK_SDK_ROOT}/Lib/dxcompiler.lib" CACHE FILEPATH "dxcompiler import lib")
set(DXC_DLL        "${VK_SDK_ROOT}/Bin/dxcompiler.dll" CACHE FILEPATH "dxcompiler runtime DLL")

if(NOT EXISTS "${DXC_LIB}")
  message(WARNING "DXC: dxcompiler.lib not found at ${DXC_LIB}")
  return()
endif()

add_library(DXC INTERFACE)

target_folder(DXC ${FOLDER_EXTERNAL})

target_include_directories(DXC INTERFACE "${DXC_INCLUDE_DIR}")
target_link_libraries(DXC INTERFACE "${DXC_LIB}")

message(STATUS "DXC: ${DXC_DLL}")
