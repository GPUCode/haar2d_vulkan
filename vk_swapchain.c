#include <stdlib.h>
#include <stdio.h>
#include "vk_device.h"
#include "vk_swapchain.h"

#include <GLFW/glfw3.h>

static const struct VkContext* context = NULL;

uint32_t clamp(uint32_t num, uint32_t min, uint32_t max) {
    return num < min ? min : (num > max ? max : num);
}

VkFormat find_present_format(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    uint32_t num_surface_formats = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_surface_formats, NULL);
    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * num_surface_formats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_surface_formats, formats);

    // If there is a single undefined surface format, the device doesn't care, so we'll just use RGBA.
    if (num_surface_formats == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        free(formats);
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    // Try to find if the physical device supports VK_FORMAT_R8G8B8A8_UNORM.
    for (uint32_t i = 0; i < num_surface_formats; i++) {
        printf("Considering surface format = %d\n", formats[i].format);
        //if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
        //    free(formats);
        //    return VK_FORMAT_R8G8B8A8_UNORM;
        //}
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            free(formats);
            return VK_FORMAT_B8G8R8A8_UNORM;
        }
    }

    free(formats);
    printf("Unable to find required swapchain format!\n");
    return VK_FORMAT_UNDEFINED;
}

void create_window(const struct VkContext* context_, struct VkWindow* out_window,
                   uint32_t width, uint32_t height) {
    context = context_;

    // Create GLFW window and create a vulkan surface from it.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, "Haar2D Demo", NULL, NULL);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(context->instance, window, NULL, &surface);
    if (result != VK_SUCCESS) {
        glfwTerminate();
        return;
    }

    const uint32_t queue_family_indices[] = { context->queue_family, context->queue_family };

    VkSurfaceCapabilitiesKHR capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physical_device, surface, &capabilities);

    VkExtent2D extent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == UINT32_MAX) {
        extent.width = clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    // Select number of images in swap chain, we prefer one buffer in the background to work on
    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        image_count = clamp(image_count, 0, capabilities.maxImageCount);
    }

    out_window->format = find_present_format(context->physical_device, surface);

    const VkSwapchainCreateInfoKHR swapchain_ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = out_window->format,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1U,
        .pQueueFamilyIndices = queue_family_indices,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = NULL,
    };

    // Create swapchain.
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    result = vkCreateSwapchainKHR(context->device, &swapchain_ci, NULL, &swapchain);
    if (result != VK_SUCCESS) {
        printf("Unable to create swapchain with result %d\b", result);
        return;
    }

    // Query the image handles.
    uint32_t num_swapchain_images = 0;
    vkGetSwapchainImagesKHR(context->device, swapchain, &num_swapchain_images, NULL);
    VkImage* images = (VkImage*)malloc(sizeof(VkImage) * num_swapchain_images);
    vkGetSwapchainImagesKHR(context->device, swapchain, &num_swapchain_images, images);

    // Create semaphores for the acquire and presentation steps.
    const VkSemaphoreCreateInfo semaphore_ci = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    const VkFenceCreateInfo fence_ci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkSemaphore* image_acquired = (VkSemaphore*)malloc(sizeof(VkSemaphore) * num_swapchain_images);
    VkSemaphore* present_ready = (VkSemaphore*)malloc(sizeof(VkSemaphore) * num_swapchain_images);
    VkFence* fences = (VkFence*)malloc(sizeof(VkFence) * num_swapchain_images);

    for (uint32_t i = 0; i < num_swapchain_images; i++) {
        vkCreateSemaphore(context->device, &semaphore_ci, NULL, &image_acquired[i]);
        vkCreateSemaphore(context->device, &semaphore_ci, NULL, &present_ready[i]);
        vkCreateFence(context->device, &fence_ci, NULL, &fences[i]);
    }

    out_window->window = window;
    out_window->surface = surface;
    out_window->swapchain = swapchain;
    out_window->width = width;
    out_window->height = height;
    out_window->num_images = num_swapchain_images;
    out_window->images = images;
    out_window->image_acquired = image_acquired;
    out_window->present_ready = present_ready;
    out_window->fences = fences;
    out_window->frame_index = 0;
}

void destroy_window(const struct VkWindow* window) {
    for (uint32_t i = 0; i < window->num_images; i++) {
        vkDestroySemaphore(context->device, window->image_acquired[i], NULL);
        vkDestroySemaphore(context->device, window->present_ready[i], NULL);
        vkDestroyFence(context->device, window->fences[i], NULL);
    }
    free(window->image_acquired);
    free(window->present_ready);
    free(window->fences);
    free(window->images);
    vkDestroySwapchainKHR(context->device, window->swapchain, NULL);
    vkDestroySurfaceKHR(context->instance, window->surface, NULL);
    glfwDestroyWindow(window->window);
}

void acquire_next_image(struct VkWindow* window) {
    const VkResult result = vkAcquireNextImageKHR(context->device, window->swapchain, UINT64_MAX,
                                                  window->image_acquired[window->frame_index],
                                                  VK_NULL_HANDLE, &window->image_index);

    if (result != VK_SUCCESS) {
        printf("Swapchain acquire returned unexpected result %d\n", result);
    }
}

void present(struct VkWindow* window) {
    const VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &window->present_ready[window->frame_index],
        .swapchainCount = 1,
        .pSwapchains = &window->swapchain,
        .pImageIndices = &window->image_index,
    };

    const VkResult result = vkQueuePresentKHR(context->queue, &present_info);
    if (result != VK_SUCCESS) {
        printf("Swapchain presentation to image %d failed with result %d\n", window->image_index, result);
        return;
    }

    window->frame_index = (window->frame_index + 1) % window->num_images;
}
