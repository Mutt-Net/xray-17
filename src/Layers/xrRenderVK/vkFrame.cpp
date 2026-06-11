#include <vulkan/vulkan.h>
#include "vkFrame.h"
#include <xrCore.h>

void VkFrameResources::Create(VkDevice device, u32 queueFamily)
{
    VkCommandPoolCreateInfo poolCI{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolCI.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCI.queueFamilyIndex = queueFamily;
    R_ASSERT(vkCreateCommandPool(device, &poolCI, nullptr, &cmdPool) == VK_SUCCESS);

    VkCommandBufferAllocateInfo allocCI{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocCI.commandPool        = cmdPool;
    allocCI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocCI.commandBufferCount = 1;
    R_ASSERT(vkAllocateCommandBuffers(device, &allocCI, &cmdBuffer) == VK_SUCCESS);

    VkSemaphoreCreateInfo semCI{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    R_ASSERT(vkCreateSemaphore(device, &semCI, nullptr, &imageAvailable) == VK_SUCCESS);
    R_ASSERT(vkCreateSemaphore(device, &semCI, nullptr, &renderFinished) == VK_SUCCESS);

    VkFenceCreateInfo fenceCI{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    R_ASSERT(vkCreateFence(device, &fenceCI, nullptr, &inFlight) == VK_SUCCESS);
}

void VkFrameResources::Destroy(VkDevice device)
{
    if (inFlight)       { vkDestroyFence(device, inFlight, nullptr);       inFlight       = VK_NULL_HANDLE; }
    if (renderFinished) { vkDestroySemaphore(device, renderFinished, nullptr); renderFinished = VK_NULL_HANDLE; }
    if (imageAvailable) { vkDestroySemaphore(device, imageAvailable, nullptr); imageAvailable = VK_NULL_HANDLE; }
    if (cmdPool)        { vkDestroyCommandPool(device, cmdPool, nullptr);   cmdPool        = VK_NULL_HANDLE; }
    cmdBuffer = VK_NULL_HANDLE;
}

void VkFrameResources::Begin()
{
    vkResetCommandBuffer(cmdBuffer, 0);

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &bi);
}

void VkFrameResources::End()
{
    vkEndCommandBuffer(cmdBuffer);
}
