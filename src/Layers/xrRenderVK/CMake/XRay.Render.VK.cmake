# XRay.Render.VK — standalone Vulkan HW support module.
#
# Provides INTERFACE include dirs and Vulkan::Vulkan link so that any module
# that adds vkHW.cpp can find <vulkan/vulkan.h> and link the Vulkan loader.
# The actual vkHW.cpp is compiled inside XRay.Render.R5VK.Refactored.HW.

add_module(XRay.Render.VK INTERFACE

  INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}

  LINKS
  Vulkan::Vulkan
)
