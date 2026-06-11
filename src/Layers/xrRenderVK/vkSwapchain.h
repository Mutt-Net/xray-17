#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include "vkAllocator.h"

struct VkSwapchainResources
{
    VkSwapchainKHR           swapchain      = VK_NULL_HANDLE;
    VkFormat                 colorFormat    = VK_FORMAT_UNDEFINED;
    VkExtent2D               extent         = {};

    xr_vector<VkImage>       images;
    xr_vector<VkImageView>   imageViews;

    VkImageAlloc             depthImage     = {};
    VkImageView              depthView      = VK_NULL_HANDLE;
    VkFormat                 depthFormat    = VK_FORMAT_D24_UNORM_S8_UINT;

    void Create(VkPhysicalDevice physDevice, VkDevice device, VkSurfaceKHR surface, u32 width, u32 height, u32 graphicsFamily);
    void Destroy(VkDevice device);
    void Recreate(VkPhysicalDevice physDevice, VkDevice device, VkSurfaceKHR surface, u32 width, u32 height, u32 graphicsFamily);

private:
    void CreateDepth(VkPhysicalDevice physDevice, VkDevice device, u32 width, u32 height);
    void DestroyDepth(VkDevice device);
    VkSurfaceFormatKHR ChooseSurfaceFormat(VkPhysicalDevice physDevice, VkSurfaceKHR surface);
    VkPresentModeKHR   ChoosePresentMode(VkPhysicalDevice physDevice, VkSurfaceKHR surface);
};
