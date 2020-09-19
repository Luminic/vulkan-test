#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <QVulkanInstance>
#include "VulkanFunctions.hpp"

// Warning: `destroy` will not be called on destructor. It must explicitly be called after `create`
class Image {
public:
    // Image creation

    class CreateData {
    public:
        uint32_t width;
        uint32_t height;
        VkFormat format;
        VkImageTiling tiling;
        VkImageUsageFlags usage;
        VkMemoryPropertyFlags properties;
        VkImageAspectFlags aspect_flags; // Needed for `create_view`

        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;

        static CreateData default_texture_data(uint32_t width=0, uint32_t height=0) {
            return CreateData{width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT,};
        }
    };
    VkResult create(VulkanData vkd, const CreateData& icd);

    static VkResult create_view(VulkanData vkd, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, VkImageView& image_view);
    // Use special `VkImageAspectFlags` instead of the one specified in create_data
    VkResult create_view(VkImageAspectFlags aspect_flags);
    VkResult create_view();

    static VkSamplerCreateInfo default_texture_sampler_create_info(VkFilter filter=VK_FILTER_NEAREST, VkSamplerAddressMode address_mode=VK_SAMPLER_ADDRESS_MODE_REPEAT, VkBool32 anisotropy_enabled=VK_FALSE);
    VkResult create_sampler();
    VkResult create_sampler(VkSamplerCreateInfo create_info);

    // Destroy all created vulkan objects & reset
    void destroy();


    // Image operations

    // Transition done in transfer stage
    static void transition_image_layout(VulkanData vkd, VkImageLayout old_layout, VkImageLayout new_layout, VkImageAspectFlags aspect_flags, VkImage image, VkCommandBuffer command_buffer);
    // Use special `VkImageAspectFlags` instead of the one specified in create_data
    void transition_image_layout(VkImageLayout old_layout, VkImageLayout new_layout, VkImageAspectFlags aspect_flags, VkCommandBuffer command_buffer);
    void transition_image_layout(VkImageLayout old_layout, VkImageLayout new_layout, VkCommandBuffer command_buffer);

    static void copy_buffer_to_image(VulkanData vkd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkCommandBuffer command_buffer);
    void copy_buffer_to_image(VkBuffer buffer, VkCommandBuffer command_buffer);


    // Getters

    VkImage get_vk_image() {return image;}
    VkDeviceMemory get_vk_image_memory() {return image_memory;}
    VkImageView get_vk_image_view() {return image_view;}
    VkSampler get_vk_sampler() {return sampler;}
    const CreateData& get_image_data() {return img_data;}
    const VkSamplerCreateInfo& get_sampler_create_info() {return sampler_create_info;}

private:
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    VulkanData vkd{};
    CreateData img_data{};
    VkSamplerCreateInfo sampler_create_info{};
};

#endif