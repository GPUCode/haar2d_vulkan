#include <stdio.h>
#include "vk_pipeline.h"
#include "vk_device.h"
#include "haar2d_hor_comp_spv.h"

void create_pipeline(const struct VkContext* context, struct VkCompPipeline* out_pipeline,
                     VkDescriptorSetLayout desc_layout) {
    out_pipeline->context = context;

    const VkShaderModuleCreateInfo shader_ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = sizeof(HAAR2D_HOR_COMP_SPV),
        .pCode = HAAR2D_HOR_COMP_SPV,
    };
    VkResult result = vkCreateShaderModule(context->device, &shader_ci, NULL, &out_pipeline->shader_module);
    if (result != VK_SUCCESS) {
        printf("Unable to create shader module with result %d\n", result);
        return;
    }

    const VkPushConstantRange push_range = {
        .stageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .offset = 0U,
        .size = sizeof(int32_t) * 2,
    };

    const VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = 1U,
        .pSetLayouts = &desc_layout,
        .pushConstantRangeCount = /*1U*/0U,
        .pPushConstantRanges = &push_range,
    };
    result = vkCreatePipelineLayout(context->device, &layout_ci, NULL, &out_pipeline->layout);
    if (result != VK_SUCCESS) {
        printf("Unable to create pipeline layout with result %d\n", result);
        return;
    }

    const VkComputePipelineCreateInfo comp_pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = out_pipeline->shader_module,
            .pName = "main",
            .pSpecializationInfo = NULL,
        },
        .layout = out_pipeline->layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };
    result = vkCreateComputePipelines(context->device, VK_NULL_HANDLE, 1U, &comp_pipeline_ci, NULL, &out_pipeline->pipeline);
    if (result != VK_SUCCESS) {
        printf("Unable to create compute pipeline with result %d\n", result);
    }
}

void destroy_pipeline(const struct VkCompPipeline* pipeline) {
    const VkDevice device = pipeline->context->device;
    vkDestroyPipeline(device, pipeline->pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline->layout, NULL);
    vkDestroyShaderModule(device, pipeline->shader_module, NULL);
}
