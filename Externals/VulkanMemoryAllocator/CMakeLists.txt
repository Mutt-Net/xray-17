add_library(VulkanMemoryAllocator STATIC vk_mem_alloc.cpp)

target_folder(VulkanMemoryAllocator ${FOLDER_EXTERNAL})

target_include_directories(VulkanMemoryAllocator
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(VulkanMemoryAllocator
  PUBLIC
    Vulkan::Vulkan
)

target_compile_options(VulkanMemoryAllocator PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W0>
)
