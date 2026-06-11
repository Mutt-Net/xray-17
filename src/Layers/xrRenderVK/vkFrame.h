#pragma once
#include <vulkan/vulkan.h>

static constexpr int VK_FRAMES_IN_FLIGHT = 2;

struct VkFrameResources
{
    VkCommandPool   cmdPool        = VK_NULL_HANDLE;
    VkCommandBuffer cmdBuffer      = VK_NULL_HANDLE;
    VkSemaphore     imageAvailable = VK_NULL_HANDLE;
    VkSemaphore     renderFinished = VK_NULL_HANDLE;
    VkFence         inFlight       = VK_NULL_HANDLE;

    void Create(VkDevice device, u32 queueFamily);
    void Destroy(VkDevice device);

    void Begin();
    void End();
};
