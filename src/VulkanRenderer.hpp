#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "VulkanFunctions.hpp"
#include "VulkanWindow.hpp"
#include "Image.hpp"

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

class VulkanRenderer : public AbstractVulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    void pre_init_resources(VulkanWindow* vulkan_window) override;
    void init_resources() override;
    void init_swap_chain_resources() override;
    void release_swap_chain_resources() override;
    void release_resources() override;

    void start_next_frame() override;

private:
    VulkanWindow* vulkan_window = nullptr;

    VulkanData vkd{};
    VkPhysicalDeviceFeatures enabled_device_features{};

    void create_descriptor_set_layout();
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;

    void create_graphics_pipeline();
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;

    void create_vertex_buffer();
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;

    void create_index_buffer();
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;

    void create_texture_image();
    Image texture_image{};
    
    void create_descriptor_pool();
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    struct FrameResources {
        VkBuffer uniform_buffer = VK_NULL_HANDLE;
        VkDeviceMemory uniform_buffer_memory = VK_NULL_HANDLE;
        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    };
    void create_uniform_buffers();
    void create_descriptor_sets();
    std::vector<FrameResources> frame_resources;

    void update_uniform_buffer(uint32_t current_frame_index);
    UniformBufferObject ubo{};


    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_properties, VkBuffer& buffer, VkDeviceMemory& memory);
    void copy_data_to_buffer(VkBuffer dst_buffer, const void* data, VkDeviceSize size);
    void copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);

    // One second fence timeout
    const uint64_t fence_timeout = 1'000'000'000;
};

#endif