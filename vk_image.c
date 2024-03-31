#include <stdio.h>
#include <string.h>
#include "vk_image.h"
#include "vk_device.h"

static uint32_t find_memory_type(const VkPhysicalDeviceMemoryProperties* properties, VkMemoryPropertyFlags wanted) {
    for (uint32_t i = 0; i < properties->memoryTypeCount; ++i) {
        const VkMemoryPropertyFlags flags = properties->memoryTypes[i].propertyFlags;
        if ((flags & wanted) == wanted) {
            return i;
        }
    }
    printf("Unable to find suitable memory type!\n");
    return UINT32_MAX;
}

void create_texture(struct VkContext* context, struct VkTexture* out_texture,
                    uint32_t width, uint32_t height, VkFormat format, VkFormat view_format) {
    out_texture->width = width;
    out_texture->height = height;
    out_texture->format = format;
    out_texture->context = context;

    const VkImageCreateInfo image_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = width,
        .extent.height = height,
        .extent.depth = 1U,
        .mipLevels = 1U,
        .arrayLayers = 1U,
        .format = format,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateImage(context->device, &image_ci, NULL, &out_texture->image);
    if (result != VK_SUCCESS) {
        printf("Unable to create image with result %d\n", result);
        return;
    }

    // Determine how much memory we need to allocate for an image.
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(context->device, out_texture->image, &requirements);

    // Query memory heaps this physical device offsers. We want to find a device local one.
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(context->physical_device, &properties);

    VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = find_memory_type(&properties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    result = vkAllocateMemory(context->device, &allocate_info, NULL, &out_texture->image_memory);
    if (result != VK_SUCCESS) {
        printf("Unable to allocate image memory with result %d\n", result);
        return;
    }

    // Back our image handle with the allocated memory
    vkBindImageMemory(context->device, out_texture->image, out_texture->image_memory, 0);

    // Create image view
    const VkImageViewCreateInfo image_view_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = out_texture->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = view_format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };
    result = vkCreateImageView(context->device, &image_view_ci, NULL, &out_texture->image_view);
    if (result != VK_SUCCESS) {
        printf("Unable to create image view with result %d\n", result);
        return;
    }

    // Since in this demo there is a single descriptor layout that contains a single image
    // let's create it here for convenience.
    const VkDescriptorSetLayoutBinding image_bindings[2] = {
        {
            .binding = 0U,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1U,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        {
            .binding = 1U,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1U,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        }
    };

    VkDescriptorSetLayoutCreateInfo desc_layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .bindingCount = 1U,
        .pBindings = image_bindings,
    };
    vkCreateDescriptorSetLayout(context->device, &desc_layout_ci, NULL, &out_texture->desc_layout);

    desc_layout_ci.bindingCount = 2U;
    vkCreateDescriptorSetLayout(context->device, &desc_layout_ci, NULL, &out_texture->desc_layout_2);

    // Also do the same for the staging buffer. In real applications this should be a global buffer
    const uint32_t buffer_size = 16 * 1024 * 1024;
    const VkBufferCreateInfo buffer_ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = buffer_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0U,
        .pQueueFamilyIndices = NULL,
    };
    result = vkCreateBuffer(context->device, &buffer_ci, NULL, &out_texture->staging_buffer);
    if (result != VK_SUCCESS) {
        printf("Unable to create buffer with result %d\n", result);
        return;
    }

    // Determine how much memory we need to allocate for our staging buffer.
    vkGetBufferMemoryRequirements(context->device, out_texture->staging_buffer, &requirements);

    // For the buffer we have specific property requirements so pass them here
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(&properties, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(context->device, &allocate_info, NULL, &out_texture->staging_memory);
    if (result != VK_SUCCESS) {
        printf("Unable to allocate staging memory with result %d\n", result);
        return;
    }

    // Back our buffer handle with the allocated memory
    vkBindBufferMemory(context->device, out_texture->staging_buffer, out_texture->staging_memory, 0);

    // Map the buffer now.
    vkMapMemory(context->device, out_texture->staging_memory, 0U, VK_WHOLE_SIZE, 0U, &out_texture->staging);
}

void transition_layout(VkCommandBuffer cmdbuf, struct VkTexture* texture, VkImageLayout new_layout,
                       VkAccessFlagBits src_access, VkAccessFlagBits dst_access,
                       VkPipelineStageFlagBits src_stage, VkPipelineStageFlagBits dst_stage) {
    const VkImageMemoryBarrier image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,
        .oldLayout = texture->layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture->image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };

    vkCmdPipelineBarrier(cmdbuf, src_stage, dst_stage, VK_DEPENDENCY_BY_REGION_BIT,
                         0U, NULL, 0U, NULL, 1U, &image_barrier);

    texture->layout = new_layout;
}

void write_as_storage_descriptor(VkDescriptorSet set, const struct VkTexture** textures, uint32_t num_textures) {
    VkDescriptorImageInfo image_infos[8];
    VkWriteDescriptorSet write_sets[8];
    for (uint32_t i = 0; i < num_textures; ++i) {
        image_infos[i].sampler = NULL;
        image_infos[i].imageView = textures[i]->image_view;
        image_infos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        write_sets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_sets[i].pNext = NULL;
        write_sets[i].dstSet = set;
        write_sets[i].dstBinding = i;
        write_sets[i].dstArrayElement = 0U;
        write_sets[i].descriptorCount = 1U;
        write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write_sets[i].pImageInfo = &image_infos[i];
        write_sets[i].pBufferInfo = NULL;
        write_sets[i].pTexelBufferView = NULL;
    }

    vkUpdateDescriptorSets(textures[0]->context->device, num_textures, write_sets, 0U, NULL);
}

void upload_image_data(VkCommandBuffer cmdbuf, uint8_t* data, uint32_t size, const struct VkTexture* texture) {
    memcpy(texture->staging, data, size);
    const VkBufferImageCopy image_copy = {
        .bufferOffset = 0U,
        .bufferRowLength = texture->width,
        .bufferImageHeight = texture->height,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0U,
            .baseArrayLayer = 0U,
            .layerCount = 1U,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {texture->width, texture->height, 1U},
    };

    vkCmdCopyBufferToImage(cmdbuf, texture->staging_buffer, texture->image, VK_IMAGE_LAYOUT_GENERAL, 1U, &image_copy);
}

void destroy_texture(const struct VkTexture* texture) {
    const VkDevice device = texture->context->device;
    vkUnmapMemory(device, texture->staging_memory);
    vkDestroyDescriptorSetLayout(device, texture->desc_layout, NULL);
    vkDestroyDescriptorSetLayout(device, texture->desc_layout_2, NULL);
    vkDestroyBuffer(device, texture->staging_buffer, NULL);
    vkDestroyImageView(device, texture->image_view, NULL);
    vkFreeMemory(device, texture->staging_memory, NULL);
    vkFreeMemory(device, texture->image_memory, NULL);
    vkDestroyImage(device, texture->image, NULL);
}
