#include "vk_device.h"
#include <stdlib.h>
#include <GLFW/glfw3.h>

#define MAX_PHYSICAL_DEVICES 8

VkInstance create_instance() {
    const VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion	= VK_MAKE_VERSION(1, 3, 0),
        .applicationVersion	= VK_MAKE_VERSION(1, 0, 0),
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .pApplicationName = "Haar2D Demo with Vulkan",
        .pEngineName = "FFmpeg",
    };

    uint32_t num_instance_extensions = 0;
    const char** instance_extensions = glfwGetRequiredInstanceExtensions(&num_instance_extensions);

    const VkInstanceCreateInfo instance_ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = num_instance_extensions,
        .ppEnabledExtensionNames = instance_extensions,
    };

    VkInstance instance	= VK_NULL_HANDLE;
    vkCreateInstance(&instance_ci, NULL, &instance);
    volkLoadInstance(instance);

    return instance;
}

uint32_t get_queue_family(VkPhysicalDevice physical_device) {
    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties* family_properties = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, family_properties);

    uint32_t graphics_queue_family = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        // This assumes queue also supports presentation, but that should hold true on any modern hardware.
        if (family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family = i;
        }
    }

    free(family_properties);
    return graphics_queue_family;
}

VkDevice create_device(VkPhysicalDevice physical_device, uint32_t graphics_queue_family) {
    const float priorities[] = { 1.0f };
    const VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .queueCount = 1,
        .queueFamilyIndex = graphics_queue_family,
        .pQueuePriorities = priorities,
    };

    const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    const VkPhysicalDeviceImageRobustnessFeatures robustness_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES,
        .pNext = NULL,
        .robustImageAccess = VK_TRUE,
    };

    const VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &robustness_features,
        .features = {
            .robustBufferAccess = VK_TRUE,
        },
    };

    const VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount	= 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions,
    };

    VkDevice device	= VK_NULL_HANDLE;
    vkCreateDevice(physical_device, &device_create_info, NULL, &device);
    volkLoadDevice(device);

    return device;
}

void create_context(struct VkContext* out_context) {
    const VkInstance instance = create_instance();

    uint32_t num_physical_devices = 0;
    vkEnumeratePhysicalDevices(instance, &num_physical_devices, NULL);
    VkPhysicalDevice physical_devices[MAX_PHYSICAL_DEVICES];
    vkEnumeratePhysicalDevices(instance, &num_physical_devices, physical_devices);

    const uint32_t index = 1;
    const uint32_t queue_family_index = get_queue_family(physical_devices[index]);
    const VkDevice device = create_device(physical_devices[index], queue_family_index);

    VkQueue queue;
    vkGetDeviceQueue(device, queue_family_index, 0, &queue);

    out_context->instance = instance;
    out_context->physical_device = physical_devices[index];
    out_context->queue_family = queue_family_index;
    out_context->device = device;
    out_context->queue = queue;
}

void destroy_context(struct VkContext* context) {
    vkDestroyDevice(context->device, NULL);
    vkDestroyInstance(context->instance, NULL);
}
