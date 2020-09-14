#include "VulkanRenderer.hpp"

VulkanRenderer::VulkanRenderer() {

}

VulkanRenderer::~VulkanRenderer() {

}

void VulkanRenderer::pre_init_resources(VulkanWindow* vulkan_window) {
    this->vulkan_window = vulkan_window;
    qDebug() << "pre_init_resources";
}

void VulkanRenderer::init_resources() {
    qDebug() << "init_resources";
}

void VulkanRenderer::init_swap_chain_resources() {
    qDebug() << "init_swap_chain_resources";
}

void VulkanRenderer::release_swap_chain_resources() {
    qDebug() << "release_swap_chain_resources";
}

void VulkanRenderer::release_resources() {
    qDebug() << "release_resources";
}

void VulkanRenderer::start_next_frame() {
    vulkan_window->frame_ready();
    qDebug() << "start_next_frame";
}
