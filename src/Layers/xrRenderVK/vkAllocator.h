#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

// Global VMA allocator — initialised by vkAllocator_Create, destroyed by vkAllocator_Destroy.
extern VmaAllocator g_vmaAllocator;

void vkAllocator_Create(VkInstance instance, VkPhysicalDevice physDevice, VkDevice device);
void vkAllocator_Destroy();

struct VkBufferAlloc
{
    VkBuffer      buffer     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
};

struct VkImageAlloc
{
    VkImage       image      = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
};

VkBufferAlloc vmaCreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage);
void          vmaDestroyBuffer(VkBufferAlloc& buf);

VkImageAlloc  vmaCreateImage(const VkImageCreateInfo& ci, VmaMemoryUsage memUsage);
void          vmaDestroyImage(VkImageAlloc& img);
