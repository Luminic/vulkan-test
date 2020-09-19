#include "Image.hpp"

#include <QDebug>

VkResult Image::create(VulkanData vkd, const CreateData& img_data) {
    this->vkd = vkd;
    this->img_data = img_data;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = img_data.width;
    image_info.extent.height = img_data.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = img_data.format;
    image_info.tiling = img_data.tiling;
    image_info.usage = img_data.usage;
    image_info.samples = img_data.sample_count;
    image_info.sharingMode = img_data.sharing_mode;
    
    VkResult res = vkd.vkdf->vkCreateImage(vkd.device, &image_info, nullptr, &image);
    if (res != VK_SUCCESS)
        return res;

    VkMemoryRequirements memory_requirements;
    vkd.vkdf->vkGetImageMemoryRequirements(vkd.device, image, &memory_requirements);

    VkMemoryAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = find_memory_type(vkd, memory_requirements.memoryTypeBits, img_data.properties);
    if (allocation_info.memoryTypeIndex == uint32_t(-1)) return VK_ERROR_UNKNOWN;

    res = vkd.vkdf->vkAllocateMemory(vkd.device, &allocation_info, nullptr, &image_memory);
    if (res != VK_SUCCESS)
        return res;

    vkd.vkdf->vkBindImageMemory(vkd.device, image, image_memory, 0);

    return VK_SUCCESS;
}

VkResult Image::create_view(VulkanData vkd, VkImage image, VkFormat format, VkImageView& image_view) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    
    return vkd.vkdf->vkCreateImageView(vkd.device, &view_info, nullptr, &image_view);
}

VkResult Image::create_view() {
    return Image::create_view(vkd, image, img_data.format, image_view);
}

inline VkSamplerCreateInfo Image::default_texture_sampler_create_info(VkFilter filter, VkSamplerAddressMode address_mode, VkBool32 anisotropy_enabled) {
    VkSamplerCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    create_info.magFilter = filter;
    create_info.minFilter = filter;
    create_info.addressModeU = address_mode;
    create_info.addressModeV = address_mode;
    create_info.addressModeW = address_mode;
    create_info.anisotropyEnable = anisotropy_enabled;
    create_info.maxAnisotropy = 16.0f;
    create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    create_info.unnormalizedCoordinates = VK_FALSE;
    create_info.compareEnable = VK_FALSE;
    create_info.compareOp = VK_COMPARE_OP_ALWAYS;
    create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    create_info.mipLodBias = 0.0f;
    create_info.minLod = 0.0f;
    create_info.maxLod = 0.0f;

    return create_info;
}

VkResult Image::create_sampler() {
    return create_sampler(Image::default_texture_sampler_create_info());
}

inline VkResult Image::create_sampler(VkSamplerCreateInfo create_info) {
    sampler_create_info = create_info;
    return vkd.vkdf->vkCreateSampler(vkd.device, &create_info, nullptr, &sampler);
}

void Image::destroy() {
    if (vkd.vkdf != nullptr) {
        vkd.vkdf->vkDestroySampler(vkd.device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
        vkd.vkdf->vkDestroyImageView(vkd.device, image_view, nullptr);
        image_view = VK_NULL_HANDLE;

        vkd.vkdf->vkDestroyImage(vkd.device, image, nullptr);
        image = VK_NULL_HANDLE;
        vkd.vkdf->vkFreeMemory(vkd.device, image_memory, nullptr);
        image_memory = VK_NULL_HANDLE;

        vkd = VulkanData{};
        img_data = CreateData{};
        sampler_create_info = VkSamplerCreateInfo{};
    }
}

void Image::transition_image_layout(VulkanData vkd, VkImageLayout old_layout, VkImageLayout new_layout, VkImage image, VkCommandBuffer command_buffer) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage;
    VkPipelineStageFlags dst_stage;

    if (old_layout==VK_IMAGE_LAYOUT_UNDEFINED && new_layout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        qFatal("Unsupported layout transition: %d to %d", old_layout, new_layout);
    }

    vkd.vkdf->vkCmdPipelineBarrier(
        command_buffer,
        src_stage, dst_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void Image::transition_image_layout(VkImageLayout old_layout, VkImageLayout new_layout, VkCommandBuffer command_buffer) {
    Image::transition_image_layout(vkd, old_layout, new_layout, image, command_buffer);
}

void Image::copy_buffer_to_image(VulkanData vkd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkCommandBuffer command_buffer) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0,0,0};
    region.imageExtent = {width, height, 1};

    vkd.vkdf->vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void Image::copy_buffer_to_image(VkBuffer buffer, VkCommandBuffer command_buffer) {
    copy_buffer_to_image(vkd, buffer, image, img_data.width, img_data.height, command_buffer);
}