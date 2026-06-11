message(STATUS)
message(STATUS "X-Ray Engine")
message(STATUS "      Version: 1.6")

cmake_policy(SET CMP0140 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS Off
  CACHE BOOL
  "Dump compiler arguments to compile_commands.json"
)

# Setup build configurations
include(XRay.Configs)

# Setup AVX support
include(XRay.AVX)

# Vulkan SDK (optional — gates Anomaly.Vulkan target)
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  message(STATUS "     Vulkan SDK: ${Vulkan_VERSION}")
else()
  message(STATUS "     Vulkan SDK: Not found — install LunarG Vulkan SDK to enable Anomaly.Vulkan")
endif()

# Setup compiler
include(XRay.Compiler)

# Setup linker
include(XRay.Linker)

# Define paths
include(XRay.Paths)

# Configure IDE folders
include(XRay.Folders)

# Setup module definition machinery
include(XRay.Modules)

# Setup source utility machinery
include(XRay.Sources)

message(STATUS)
