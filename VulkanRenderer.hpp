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
};

#endif