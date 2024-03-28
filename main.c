#include <volk.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "vk_device.h"
#include "vk_swapchain.h"
#include "vk_image.h"
#include "vk_pipeline.h"
#include "stb_image.h"
#include "haar2d_hor_comp_spv.h"
#include <GLFW/glfw3.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_FRAMES 6

struct Vec2i {
    int32_t x;
    int32_t y;
};

int main() {
    glfwInit();
    if (!glfwVulkanSupported()) {
        glfwTerminate();
        return 1;
    }
    if (volkInitialize() != VK_SUCCESS) {
        glfwTerminate();
        return 1;
    }

    // Create context and window.
    struct VkContext context = {};
    struct VkWindow window = {};
    struct VkTexture texture = {};
    struct VkCompPipeline pipeline = {};
    create_context(&context);
    create_window(&context, &window, WIDTH, HEIGHT);
    create_texture(&context, &texture, WIDTH, HEIGHT, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM);
    create_pipeline(&context, &pipeline, texture.desc_layout, HAAR2D_HOR_COMP_SPV, sizeof(HAAR2D_HOR_COMP_SPV));

    // Make command pool to allocate command buffers.
    const VkCommandPoolCreateInfo command_pool_ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.queue_family,
    };

    VkCommandPool command_pool = VK_NULL_HANDLE;
    vkCreateCommandPool(context.device, &command_pool_ci, NULL, &command_pool);

    const VkCommandBufferAllocateInfo buffer_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = window.num_images,
    };

    VkCommandBuffer buffers[MAX_FRAMES];
    vkAllocateCommandBuffers(context.device, &buffer_alloc_info, buffers);

    // Make a descriptor pool to allocate the storage image descriptors we need.
    const VkDescriptorPoolSize pool_size = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16};
    const VkDescriptorPoolCreateInfo descriptor_pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .maxSets = 8,
        .poolSizeCount = 1U,
        .pPoolSizes = &pool_size,
    };

    VkDescriptorPool desc_pool;
    vkCreateDescriptorPool(context.device, &descriptor_pool_ci, NULL, &desc_pool);

    // Allocate one descriptor for each frame.
    VkDescriptorSetLayout layouts[MAX_FRAMES];
    for (uint32_t i = 0; i < window.num_images; ++i) {
        layouts[i] = texture.desc_layout;
    }

    const VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = desc_pool,
        .descriptorSetCount = window.num_images,
        .pSetLayouts = layouts,
    };

    VkDescriptorSet desc_sets[MAX_FRAMES];
    vkAllocateDescriptorSets(context.device, &allocate_info, desc_sets);

    // Update allocated sets with our image.
    write_as_storage_descriptor(window.num_images, desc_sets, &texture);

    // Load test image
    int32_t width, height, num_channels;
    uint8_t* data = stbi_load("ffmpeg_6.1.1.png", &width, &height, &num_channels, 0);

    const VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };

    bool texture_initialized = false;
    while (!glfwWindowShouldClose(window.window)) {
        glfwPollEvents();

        // Retrieve command buffer for this loop.
        VkCommandBuffer cmdbuf = buffers[window.frame_index];

        // Wait for the fence signaled on queue submit. This ensures our command buffer is free to use.
        vkWaitForFences(context.device, 1U, &window.fences[window.frame_index], VK_FALSE, UINT64_MAX);
        vkResetFences(context.device, 1U, &window.fences[window.frame_index]);

        // Acquire a new swapchain image to draw on
        acquire_next_image(&window);

        // Begin the command buffer.
        const VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmdbuf, &begin_info);

        // Transition from undefined to general and upload pixel data in the first frame.
        if (!texture_initialized) {
            transition_layout(cmdbuf, &texture, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE, VK_ACCESS_NONE,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

            // Upload test image to vulkan image.
            upload_image_data(cmdbuf, data, width * height * num_channels, &texture);

            transition_layout(cmdbuf, &texture, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            // Bind descriptor sets and pipeline.
            vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.layout, 0U, 1U,
                                    &desc_sets[window.frame_index], 0U, NULL);
            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);

            uint32_t width = WIDTH / 32;
            uint32_t height = HEIGHT / 32;
            for (uint32_t i = 0; i < 2; i++) {
                struct PushConstants con = {.block_dim = 128, .level = i};
                vkCmdPushConstants(cmdbuf, pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0U, sizeof(con), &con);
                vkCmdDispatch(cmdbuf, width, height, 1);
            }
            texture_initialized = true;
        }

        // Transition swapchain image to transfer dest layout for clearing.
        VkImageMemoryBarrier image_barriers[2] = {
            // Image barrier for the swapchain image.
            [0] = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_NONE,
                .dstAccessMask = VK_ACCESS_NONE,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = window.images[window.frame_index],
                .subresourceRange = range,
            },
            // Image barrier for the storage image.
            [1] = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = texture.image,
                .subresourceRange = range,
            }
        };

        vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT, 0U, NULL, 0U, NULL, 2U, image_barriers);

        // Copy the resulting image to the swapchain. The format is the same with the swapchain.
        const VkImageBlit image_copy = {
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1U,
            },
            .srcOffsets = {{0, 0, 0}, {WIDTH, HEIGHT, 1}},
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1U,
            },
            .dstOffsets = {{0, 0, 0}, {WIDTH, HEIGHT, 1}},
        };
        vkCmdBlitImage(cmdbuf, texture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       window.images[window.frame_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1U, &image_copy, VK_FILTER_NEAREST);

        // Transition to presentation layout after we are done.
        image_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_barriers[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        image_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // Transition image back to general layout for next frame.
        image_barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        image_barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        image_barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT, 0U, NULL, 0U, NULL, 2U, image_barriers);

        // End command buffer and submit.
        vkEndCommandBuffer(cmdbuf);

        const VkPipelineStageFlags wait_flags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        const VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 1U,
            .pWaitSemaphores = &window.image_acquired[window.frame_index],
            .pWaitDstStageMask = &wait_flags,
            .commandBufferCount = 1U,
            .pCommandBuffers = &cmdbuf,
            .signalSemaphoreCount = 1U,
            .pSignalSemaphores = &window.present_ready[window.frame_index],
        };
        vkQueueSubmit(context.queue, 1U, &submit_info, window.fences[window.frame_index]);

        // Present
        present(&window);
    }

    // Wait for all the fences to ensure all command buffers have finished execution.
    vkWaitForFences(context.device, window.num_images, window.fences, VK_TRUE, UINT64_MAX);
    vkDestroyDescriptorPool(context.device, desc_pool, NULL);
    vkDestroyCommandPool(context.device, command_pool, NULL);
    stbi_image_free(data);

    // Cleanup.
    destroy_pipeline(&pipeline);
    destroy_texture(&texture);
    destroy_window(&window);
    destroy_context(&context);
    glfwTerminate();
    return 0;
}
