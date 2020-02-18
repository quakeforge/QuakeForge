#ifndef __vid_vulkan_h
#define __vid_vulkan_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

typedef struct vulkan_frameset_s {
	int         curFrame;	// index into fences
	struct qfv_fenceset_s *fences;
	struct qfv_cmdbufferset_s *cmdBuffers;
	struct qfv_semaphoreset_s *imageSemaphores;
	struct qfv_semaphoreset_s *renderDoneSemaphores;
} vulkan_frameset_t;

typedef struct vulkan_renderpass_s {
	struct qfv_imageresource_s *colorImage;
	struct qfv_imageresource_s *depthImage;
} vulkan_renderpass_t;

typedef struct vulkan_ctx_s {
	void        (*load_vulkan) (struct vulkan_ctx_s *ctx);
	void        (*unload_vulkan) (struct vulkan_ctx_s *ctx);

	const char **required_extensions;
	struct vulkan_presentation_s *presentation;
	int (*get_presentation_support) (struct vulkan_ctx_s *ctx,
									 VkPhysicalDevice physicalDevice,
									 uint32_t queueFamilyIndex);
	void        (*choose_visual) (struct vulkan_ctx_s *ctx);
	void        (*create_window) (struct vulkan_ctx_s *ctx);
	VkSurfaceKHR (*create_surface) (struct vulkan_ctx_s *ctx);

	struct qfv_instance_s *instance;
	struct qfv_device_s *device;
	struct qfv_swapchain_s *swapchain;
	VkSurfaceKHR surface;	//FIXME surface = window, so "contains" swapchain

	VkCommandPool cmdpool;
	VkCommandBuffer cmdbuffer;
	VkFence     fence;			// for ctx->cmdbuffer only
	vulkan_frameset_t frameset;
	vulkan_renderpass_t renderpass;

#define EXPORTED_VULKAN_FUNCTION(fname) PFN_##fname fname;
#define GLOBAL_LEVEL_VULKAN_FUNCTION(fname) PFN_##fname fname;
#include "QF/Vulkan/funclist.h"
} vulkan_ctx_t;

#endif//__vid_vulkan_h
