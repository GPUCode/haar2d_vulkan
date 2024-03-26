#pragma once

#include <volk.h>

struct VkContext;

struct VkCompPipeline {
    const struct VkContext* context;
    VkPipelineLayout layout;
    VkPipeline pipeline;
    VkShaderModule shader_module;
};

struct PushConstants {
    int level;
    int block_dim;
};

void create_pipeline(const struct VkContext* context, struct VkCompPipeline* out_pipeline,
                     VkDescriptorSetLayout desc_layout, const uint32_t* code, uint32_t code_size);

void destroy_pipeline(const struct VkCompPipeline* pipeline);
