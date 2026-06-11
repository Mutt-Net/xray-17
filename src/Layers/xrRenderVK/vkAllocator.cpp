#include "vkAllocator.h"

VmaAllocator g_vmaAllocator = VK_NULL_HANDLE;

void vkAllocator_Create(VkInstance instance, VkPhysicalDevice physDevice, VkDevice device)
{
    VmaAllocatorCreateInfo ci{};
    ci.instance       = instance;
    ci.physicalDevice = physDevice;
    ci.device         = device;
    ci.vulkanApiVersion = VK_API_VERSION_1_1;
    vmaCreateAllocator(&ci, &g_vmaAllocator);
}

void vkAllocator_Destroy()
{
    if (g_vmaAllocator)
    {
        vmaDestroyAllocator(g_vmaAllocator);
        g_vmaAllocator = VK_NULL_HANDLE;
    }
}

VkBufferAlloc vmaCreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage)
{
    VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size  = size;
    bci.usage = usage;

    VmaAllocationCreateInfo aci{};
    aci.usage = memUsage;

    VkBufferAlloc result{};
    vmaCreateBuffer(g_vmaAllocator, &bci, &aci, &result.buffer, &result.allocation, nullptr);
    return result;
}

void vmaDestroyBuffer(VkBufferAlloc& buf)
{
    if (buf.buffer)
    {
        vmaDestroyBuffer(g_vmaAllocator, buf.buffer, buf.allocation);
        buf.buffer     = VK_NULL_HANDLE;
        buf.allocation = VK_NULL_HANDLE;
    }
}

VkImageAlloc vmaCreateImage(const VkImageCreateInfo& ci, VmaMemoryUsage memUsage)
{
    VmaAllocationCreateInfo aci{};
    aci.usage = memUsage;

    VkImageAlloc result{};
    vmaCreateImage(g_vmaAllocator, &ci, &aci, &result.image, &result.allocation, nullptr);
    return result;
}

void vmaDestroyImage(VkImageAlloc& img)
{
    if (img.image)
    {
        vmaDestroyImage(g_vmaAllocator, img.image, img.allocation);
        img.image      = VK_NULL_HANDLE;
        img.allocation = VK_NULL_HANDLE;
    }
}
