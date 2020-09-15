#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#include "VulkanWindow.hpp"

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

    QVulkanFunctions* vkf = nullptr;
    QVulkanDeviceFunctions* vkdf = nullptr;
    VkDevice device = VK_NULL_HANDLE;

    void create_graphics_pipeline();
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;

    void create_vertex_buffer();
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;

    void create_index_buffer();
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;


    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer command_buffer);

    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);

    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_properties, VkBuffer& buffer, VkDeviceMemory& memory);
    void copy_data_to_buffer(VkBuffer dst_buffer, const void* data, VkDeviceSize size);
    void copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);

    // One second fence timeout
    const int fence_timeout = 1'000'000'000;
};

#endif