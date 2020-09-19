#include <QApplication>
#include <QVulkanInstance>
#include <QLoggingCategory>

#include "VulkanWindow.hpp"
#include "VulkanRenderer.hpp"


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

    QVulkanInstance inst;
    inst.setApiVersion(QVersionNumber(1,2));
    inst.setLayers(QByteArrayList() << "VK_LAYER_LUNARG_standard_validation" << "VK_LAYER_KHRONOS_validation");

    if (!inst.create())
        qFatal("Failed to create Vulkan instance: %d", inst.errorCode());

    VulkanRenderer* vulkan_renderer = new VulkanRenderer();
    VulkanWindow vulkan_window(vulkan_renderer);
    vulkan_window.setVulkanInstance(&inst);
    vulkan_window.resize(800,600);
    vulkan_window.show();

    return app.exec();
}