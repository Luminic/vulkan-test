#ifndef VULKAN_FUNCTIONS_HPP
#define VULKAN_FUNCTIONS_HPP

#include <QVulkanInstance>
#include <QVulkanDeviceFunctions>

struct VulkanData {
    QVulkanInstance* instance = nullptr;
    QVulkanFunctions* vkf = nullptr;
    QVulkanDeviceFunctions* vkdf = nullptr;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
};


uint32_t find_memory_type(VulkanData vkd, uint32_t type_filter, VkMemoryPropertyFlags properties);


VkCommandBuffer begin_single_time_commands(VulkanData vkd, VkCommandPool command_pool);
void end_single_time_commands(VulkanData vkd, VkCommandPool command_pool, VkQueue queue, VkCommandBuffer command_buffer, uint64_t fence_timeout=1'000'000'000);


#endif