#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include "vkSwapchain.h"
#include <xrCore.h>
#include <algorithm>

VkSurfaceFormatKHR VkSwapchainResources::ChooseSurfaceFormat(VkPhysicalDevice physDevice, VkSurfaceKHR surface)
{
    u32 count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &count, nullptr);
    xr_vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &count, formats.data());

    for (auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return formats[0];
}

VkPresentModeKHR VkSwapchainResources::ChoosePresentMode(VkPhysicalDevice physDevice, VkSurfaceKHR surface)
{
    u32 count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &count, nullptr);
    xr_vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &count, modes.data());

    for (auto m : modes)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

void VkSwapchainResources::CreateDepth(VkPhysicalDevice physDevice, VkDevice device, u32 width, u32 height)
{
    // Pick best supported depth format
    VkFormat candidates[] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT };
    for (VkFormat fmt : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physDevice, fmt, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depthFormat = fmt;
            break;
        }
    }

    VkImageCreateInfo ci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.format        = depthFormat;
    ci.extent        = { width, height, 1 };
    ci.mipLevels     = 1;
    ci.arrayLayers   = 1;
    ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ci.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    depthImage = vmaCreateImage(ci, VMA_MEMORY_USAGE_GPU_ONLY);

    VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vci.image                           = depthImage.image;
    vci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    vci.format                          = depthFormat;
    vci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    vci.subresourceRange.baseMipLevel   = 0;
    vci.subresourceRange.levelCount     = 1;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount     = 1;
    R_ASSERT(vkCreateImageView(device, &vci, nullptr, &depthView) == VK_SUCCESS);
}

void VkSwapchainResources::DestroyDepth(VkDevice device)
{
    if (depthView)  { vkDestroyImageView(device, depthView, nullptr); depthView = VK_NULL_HANDLE; }
    vmaDestroyImage(depthImage);
}

void VkSwapchainResources::Create(VkPhysicalDevice physDevice, VkDevice device, VkSurfaceKHR surface,
                                   u32 width, u32 height, u32 graphicsFamily)
{
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &caps);

    auto fmt  = ChooseSurfaceFormat(physDevice, surface);
    auto mode = ChoosePresentMode(physDevice, surface);

    colorFormat = fmt.format;
    extent.width  = std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);

    u32 imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        imgCount = std::min(imgCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR sci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    sci.surface               = surface;
    sci.minImageCount         = imgCount;
    sci.imageFormat           = fmt.format;
    sci.imageColorSpace       = fmt.colorSpace;
    sci.imageExtent           = extent;
    sci.imageArrayLayers      = 1;
    sci.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    sci.queueFamilyIndexCount = 1;
    sci.pQueueFamilyIndices   = &graphicsFamily;
    sci.preTransform          = caps.currentTransform;
    sci.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode           = mode;
    sci.clipped               = VK_TRUE;
    R_ASSERT(vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain) == VK_SUCCESS);

    // Get swapchain images
    u32 n = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &n, nullptr);
    images.resize(n);
    vkGetSwapchainImagesKHR(device, swapchain, &n, images.data());

    imageViews.resize(n);
    for (u32 i = 0; i < n; i++)
    {
        VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image                           = images[i];
        vci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        vci.format                          = colorFormat;
        vci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.baseMipLevel   = 0;
        vci.subresourceRange.levelCount     = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount     = 1;
        R_ASSERT(vkCreateImageView(device, &vci, nullptr, &imageViews[i]) == VK_SUCCESS);
    }

    CreateDepth(physDevice, device, extent.width, extent.height);
}

void VkSwapchainResources::Destroy(VkDevice device)
{
    DestroyDepth(device);
    for (auto& v : imageViews)
        vkDestroyImageView(device, v, nullptr);
    imageViews.clear();
    images.clear();
    if (swapchain) { vkDestroySwapchainKHR(device, swapchain, nullptr); swapchain = VK_NULL_HANDLE; }
}

void VkSwapchainResources::Recreate(VkPhysicalDevice physDevice, VkDevice device, VkSurfaceKHR surface,
                                     u32 width, u32 height, u32 graphicsFamily)
{
    vkDeviceWaitIdle(device);
    Destroy(device);
    Create(physDevice, device, surface, width, height, graphicsFamily);
}
