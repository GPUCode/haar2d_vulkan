#pragma once

#include <volk.h>

struct VkContext;
typedef struct GLFWwindow GLFWwindow;

struct VkWindow {
    VkSurfaceKHR surface;
    GLFWwindow* window;
    VkSwapchainKHR swapchain;
    uint32_t width;
    uint32_t height;
    uint32_t num_images;
    VkImage* images;
    VkSemaphore* image_acquired;
    VkSemaphore* present_ready;
    VkFence* fences;
    uint32_t frame_index;
    uint32_t image_index;
};

void create_window(const struct VkContext* context, struct VkWindow* out_window,
                   uint32_t width, uint32_t height);

void destroy_window(const struct VkWindow* window);

void acquire_next_image(struct VkWindow* window);

void present(struct VkWindow* window);
