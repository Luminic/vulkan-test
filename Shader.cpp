#include "Shader.hpp"

#include <QVulkanDeviceFunctions>
#include <QFile>

ShaderModule::ShaderModule(const VkDevice& device, QVulkanDeviceFunctions* vkdf, const QString& path) {
    initialize(device, vkdf);
    load(path);
}

ShaderModule::~ShaderModule() {
    // Make sure not to segfault when an uninitialized ShaderModule is destroyed
    if (vkdf) {
        destroy();
    }
}

void ShaderModule::initialize(const VkDevice& device, QVulkanDeviceFunctions* vkdf) {
    this->device = device;
    this->vkdf = vkdf;
}

void ShaderModule::load(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read shader %s", qPrintable(path));
        return;
    }
    QByteArray blob = file.readAll();
    file.close();

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = blob.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(blob.constData());

    VkResult res = vkdf->vkCreateShaderModule(device, &create_info, nullptr, &vk_shader_module);
    if (res != VK_SUCCESS) {
        qWarning("Failed to create shader module: %d", res);
    }
}

VkPipelineShaderStageCreateInfo ShaderModule::get_create_info(const VkShaderStageFlagBits& flag_bits) {
    VkPipelineShaderStageCreateInfo shader_stage_create_info{};
    shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_info.stage = flag_bits;
    shader_stage_create_info.module = vk_shader_module;
    shader_stage_create_info.pName = "main";
    return shader_stage_create_info;
}

void ShaderModule::destroy() {
    vkdf->vkDestroyShaderModule(device, vk_shader_module, nullptr);
    device = VK_NULL_HANDLE;
    vkdf = nullptr;
}