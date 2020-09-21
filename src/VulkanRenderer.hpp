#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <QElapsedTimer>

#include "VulkanFunctions.hpp"
#include "VulkanWindow.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

#include "settings/ControlPanel.hpp"

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

    // Creates both vertex and index buffers
    void create_vertex_buffer();
    Buffer vertex_buffer{};
    Buffer index_buffer{};

    void create_texture_image();
    Image texture_image{};
    
    void create_descriptor_pool();
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    void create_uniform_buffers();
    Buffer uniform_buffer{};
    VkDeviceSize aligned_size = 0;
    uchar* uniform_buffer_memory_ptr = nullptr;

    void create_descriptor_sets();
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    void update_uniform_buffer(uint32_t current_frame_index);
    UniformBufferObject ubo{};


    // One second fence timeout
    const uint64_t fence_timeout = 1'000'000'000;

    QElapsedTimer fps_timer;
    ControlPanel control_panel;
};

#endif