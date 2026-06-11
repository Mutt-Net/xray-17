// r2_test_hw_vk.cpp — Vulkan hardware capability probe for R5VK.
// Tests for Vulkan 1.1 support (minimum for D3D11-feature-level equivalent).
#include <vulkan/vulkan.h>

BOOL xrRender_test_hw()
{
	VkApplicationInfo appInfo{};
	appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName   = "STALKER Anomaly R5VK probe";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion         = VK_API_VERSION_1_1;

	VkInstanceCreateInfo ci{};
	ci.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &appInfo;

	VkInstance inst = VK_NULL_HANDLE;
	VkResult    res = vkCreateInstance(&ci, nullptr, &inst);

	if (res != VK_SUCCESS) {
		Msg("* R5VK: vkCreateInstance failed (VkResult=%d) — Vulkan unavailable", (int)res);
		return FALSE;
	}

	// Check for at least one physical device supporting Vulkan 1.1
	u32 devCount = 0;
	vkEnumeratePhysicalDevices(inst, &devCount, nullptr);
	if (devCount == 0) {
		Msg("* R5VK: No Vulkan-capable GPU found");
		vkDestroyInstance(inst, nullptr);
		return FALSE;
	}

	Msg("* R5VK: Vulkan hardware probe passed (%u device(s))", devCount);
	vkDestroyInstance(inst, nullptr);
	return TRUE;
}
