#pragma once

#include <volk.h>

struct VkContext;

struct VkCompPipeline {
    const struct VkContext* context;
    VkPipelineLayout layout;
    VkPipeline pipeline;
    VkShaderModule shader_module;
};

void create_pipeline(const struct VkContext* context, struct VkCompPipeline* out_pipeline,
                     VkDescriptorSetLayout desc_layout);

void destroy_pipeline(const struct VkCompPipeline* pipeline);
