#include "VulkanFunctions.hpp"

uint32_t find_memory_type(VulkanData vkd, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkd.vkf->vkGetPhysicalDeviceMemoryProperties(vkd.physical_device, &memory_properties);
    for (uint32_t i=0; i<memory_properties.memoryTypeCount; i++) {
        if (type_filter & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    return -1;
}

VkCommandBuffer begin_single_time_commands(VulkanData vkd, VkCommandPool command_pool) {
    VkCommandBufferAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation_info.commandPool = command_pool;
    allocation_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkd.vkdf->vkAllocateCommandBuffers(vkd.device, &allocation_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkd.vkdf->vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void end_single_time_commands(VulkanData vkd, VkCommandPool command_pool, VkQueue queue, VkCommandBuffer command_buffer, uint64_t fence_timeout) {
    vkd.vkdf->vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    vkd.vkdf->vkCreateFence(vkd.device, &fence_create_info, nullptr, &fence);

    vkd.vkdf->vkQueueSubmit(queue, 1, &submit_info, fence);

    VkResult res = vkd.vkdf->vkWaitForFences(vkd.device, 1, &fence, VK_FALSE, fence_timeout);
    if (res != VK_SUCCESS)
        qWarning("Timeout waiting for fence: %d", res);
    
    vkd.vkdf->vkFreeCommandBuffers(vkd.device, command_pool, 1, &command_buffer);
    vkd.vkdf->vkDestroyFence(vkd.device, fence, nullptr);
}

VkResult create_image(VulkanData vkd, const ImageCreateData& icd, VkImage& image, VkDeviceMemory& image_memory) {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = icd.width;
    image_info.extent.height = icd.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = icd.format;
    image_info.tiling = icd.tiling;
    image_info.usage = icd.usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkResult res = vkd.vkdf->vkCreateImage(vkd.device, &image_info, nullptr, &image);
    if (res != VK_SUCCESS)
        return res;

    VkMemoryRequirements memory_requirements;
    vkd.vkdf->vkGetImageMemoryRequirements(vkd.device, image, &memory_requirements);

    VkMemoryAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = find_memory_type(vkd, memory_requirements.memoryTypeBits, icd.properties);
    if (allocation_info.memoryTypeIndex == uint32_t(-1)) return VK_ERROR_UNKNOWN;

    res = vkd.vkdf->vkAllocateMemory(vkd.device, &allocation_info, nullptr, &image_memory);
    if (res != VK_SUCCESS)
        return res;

    vkd.vkdf->vkBindImageMemory(vkd.device, image, image_memory, 0);

    return VK_SUCCESS;
}

void transition_image_layout(VulkanData vkd, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, VkImage image, VkCommandPool command_pool, VkQueue queue, uint64_t fence_timeout) {
    VkCommandBuffer command_buffer = begin_single_time_commands(vkd, command_pool);

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

    end_single_time_commands(vkd, command_pool, queue, command_buffer, fence_timeout);
}

void copy_buffer_to_image(VulkanData vkd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkCommandPool command_pool, VkQueue queue, uint64_t fence_timeout) {
    VkCommandBuffer command_buffer = begin_single_time_commands(vkd, command_pool);

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

    end_single_time_commands(vkd, command_pool, queue, command_buffer, fence_timeout);
}