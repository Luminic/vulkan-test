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


class ImageCreateData {
public:
    uint32_t width;
    uint32_t height;
    VkFormat format;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags properties;

    static ImageCreateData default_texture_image(uint32_t width=0, uint32_t height=0) {
        return ImageCreateData{width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,};
    }
};
VkResult create_image(VulkanData vkd, const ImageCreateData& icd, VkImage& image, VkDeviceMemory& image_memory);

void transition_image_layout(
    VulkanData vkd,
    VkFormat format,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkImage image,
    VkCommandPool command_pool,
    VkQueue queue,
    uint64_t fence_timeout=1'000'000'000
);

void copy_buffer_to_image(
    VulkanData vkd,
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height,
    VkCommandPool command_pool,
    VkQueue queue,
    uint64_t fence_timeout
);

#endif