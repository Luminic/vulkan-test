#include "VulkanRenderer.hpp"

#include <QVulkanDeviceFunctions>

VulkanRenderer::VulkanRenderer() {

}

VulkanRenderer::~VulkanRenderer() {

}

void VulkanRenderer::pre_init_resources(VulkanWindow* vulkan_window) {
    qDebug() << "pre_init_resources";
    this->vulkan_window = vulkan_window;
}

void VulkanRenderer::init_resources() {
    qDebug() << "init_resources";
    device = vulkan_window->get_device();
    vkf = vulkan_window->vulkanInstance()->functions();
    vkdf = vulkan_window->vulkanInstance()->deviceFunctions(device);
}

void VulkanRenderer::init_swap_chain_resources() {
    qDebug() << "init_swap_chain_resources";
}

void VulkanRenderer::release_swap_chain_resources() {
    qDebug() << "release_swap_chain_resources";
}

void VulkanRenderer::release_resources() {
    qDebug() << "release_resources";
    device = VK_NULL_HANDLE;
    vkf = nullptr;
    vkdf = nullptr;
}

void VulkanRenderer::start_next_frame() {
    static float green = 0.0f;
    green += 0.005f;
    if (green > 1.0f) green -= 1.0f;
    VkClearColorValue clear_color = {{0.0f, green, 0.0f, 1.0f}};
    VkClearValue clear_value{};
    clear_value.color = clear_color;

    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = vulkan_window->get_default_render_pass();
    render_pass_begin_info.framebuffer = vulkan_window->get_current_frame_buffer();
    render_pass_begin_info.renderArea.extent = vulkan_window->get_image_extent();
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;

    // We're not rendering anything rn: we're just clearing the screen
    vkdf->vkCmdBeginRenderPass(vulkan_window->get_current_command_buffer(), &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkdf->vkCmdEndRenderPass(vulkan_window->get_current_command_buffer());

    vulkan_window->frame_ready();
    vulkan_window->requestUpdate();
}
