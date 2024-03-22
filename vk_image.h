#pragma once

#include <volk.h>

struct VkContext;

struct VkTexture {
    struct VkContext* context;
    uint32_t width;
    uint32_t height;
    VkFormat format;
    VkDescriptorSetLayout desc_layout;
    VkImage image;
    VkImageView image_view;
    VkImageLayout layout;
    VkDeviceMemory image_memory;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    void* staging;
};

void create_texture(struct VkContext* context, struct VkTexture* out_texture,
                    uint32_t width, uint32_t height, VkFormat format, VkFormat view_format);

void transition_layout(VkCommandBuffer cmdbuf, struct VkTexture* texture, VkImageLayout new_layout,
                       VkAccessFlagBits src_access, VkAccessFlagBits dst_access,
                       VkPipelineStageFlagBits src_stage, VkPipelineStageFlagBits dst_stage);

void write_as_storage_descriptor(uint32_t num_sets, VkDescriptorSet* sets, const struct VkTexture* texture);

void upload_image_data(VkCommandBuffer cmdbuf, uint8_t* data, uint32_t size, const struct VkTexture* texture);

void destroy_texture(const struct VkTexture* texture);
