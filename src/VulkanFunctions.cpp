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
