#include <QVulkanInstance>

class ShaderModule {
public:
    ShaderModule() = default;
    ShaderModule(const VkDevice& device, QVulkanDeviceFunctions* vkdf, const QString& path);
    ~ShaderModule();

    void initialize(const VkDevice& device, QVulkanDeviceFunctions* vkdf);
    void load(const QString& path);

    VkPipelineShaderStageCreateInfo get_create_info(const VkShaderStageFlagBits& flag_bits);

    VkShaderModule vk_shader_module = VK_NULL_HANDLE;

private:
    void destroy();

    VkDevice device = VK_NULL_HANDLE;
    QVulkanDeviceFunctions* vkdf = nullptr;
};

