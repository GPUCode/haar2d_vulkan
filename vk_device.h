#pragma once

#include <volk.h>

struct VkContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t queue_family;
    VkQueue queue;
};

void create_context(struct VkContext* out_context);

void destroy_context(struct VkContext* context);
