#include "VulkanWindow.hpp"

#include <QVulkanFunctions>
#include <QPlatformSurfaceEvent>

#include <set>
#include <string>
#include <algorithm>

#include "Image.hpp"

VulkanWindow::VulkanWindow(AbstractVulkanRenderer* vulkan_renderer, QWindow* parent) :
    QWindow(parent),
    vulkan_renderer(vulkan_renderer)
{
    setSurfaceType(QSurface::VulkanSurface);
}

VulkanWindow::~VulkanWindow() {
    delete vulkan_renderer;
}


// Private Functions:
//===================

void VulkanWindow::init_resources() {
    vulkan_renderer->pre_init_resources(this);

    vkd.instance = vulkanInstance();
    vkd.vkf = vkd.instance->functions();
    resolve_instance_extension_functions();

    create_surface();
    pick_physical_device();
    create_logical_device();

    vkd.vkdf = vkd.instance->deviceFunctions(vkd.device);
    resolve_device_extension_functions();

    create_queues();
    create_command_pool();

    // Figure these out here because we want the renderpass to be available at init_resources
    swap_chain_support_details = query_swap_chain_support_details(vkd.physical_device);
    swap_chain_surface_format = choose_swap_surface_format(swap_chain_support_details.formats);
    depth_stencil_format = find_depth_format();
    has_stencil = has_stencil_component(depth_stencil_format);

    // (So the pipeline can be created in init_resources and doesn't have to be defered to init_swap_chain_resources)
    create_default_render_pass();

    status = Status::Device_Ready;
    vulkan_renderer->init_resources();
}

void VulkanWindow::init_swap_chain_resources() {
    // Update swap_chain support details
    swap_chain_support_details = query_swap_chain_support_details(vkd.physical_device);

    create_swap_chain();
    create_depth_image();
    get_swap_chain_images();
    create_frame_buffers();

    create_sync_objects();

    status = Status::Ready;
    vulkan_renderer->init_swap_chain_resources();
}

void VulkanWindow::release_swap_chain_resources() {
    vkd.vkdf->vkDeviceWaitIdle(vkd.device);

    status = Status::Device_Ready;
    vulkan_renderer->release_swap_chain_resources();

    depth_image.destroy();

    for (auto& frame_resource : frame_resources) {
        vkd.vkdf->vkDestroySemaphore(vkd.device, frame_resource.image_available_semaphore, nullptr);
        frame_resource.image_available_semaphore = VK_NULL_HANDLE;
        vkd.vkdf->vkDestroySemaphore(vkd.device, frame_resource.render_finished_semaphore, nullptr);
        frame_resource.render_finished_semaphore = VK_NULL_HANDLE;
        vkd.vkdf->vkDestroyFence(vkd.device, frame_resource.fence, nullptr);
        frame_resource.fence = VK_NULL_HANDLE;
    }

    for (auto& image_resource : image_resources) {
        vkd.vkdf->vkDestroyImageView(vkd.device, image_resource.image_view, nullptr);
        image_resource.image_view = VK_NULL_HANDLE;
        vkd.vkdf->vkDestroyFramebuffer(vkd.device, image_resource.framebuffer, nullptr);
        image_resource.framebuffer = VK_NULL_HANDLE;
        vkd.vkdf->vkFreeCommandBuffers(vkd.device, command_pool, 1, &image_resource.command_buffer);
        image_resource.command_buffer = VK_NULL_HANDLE;
        // The fence should be the same fence as one in frame_resources so no need
        // to call vkDestroyFence; just reset the handle
        image_resource.fence = VK_NULL_HANDLE;
    }

    vkDestroySwapchainKHR(vkd.device, swap_chain, nullptr);
    swap_chain = VK_NULL_HANDLE;
}

void VulkanWindow::release_resources() {
    status = Status::Uninitialized;
    vulkan_renderer->release_resources();

    vkd.vkdf->vkDestroyCommandPool(vkd.device, command_pool, nullptr);
    command_pool = VK_NULL_HANDLE;

    vkd.vkdf->vkDestroyRenderPass(vkd.device, default_render_pass, nullptr);
    default_render_pass = VK_NULL_HANDLE;

    vkd.vkdf->vkDestroyDevice(vkd.device, nullptr);
    vkd.device = VK_NULL_HANDLE;

    vkd.vkf = nullptr;
    vkd.vkdf = nullptr;
}

void VulkanWindow::begin_frame() {
    if (status != Status::Ready) {
        qWarning("VulkanWindow: Tried to begin frame with incomplete initialization: %d", int(status));
        return;
    }

    VkResult res;
    FrameResources& frame_resource = frame_resources[frame_index];

    // qDebug() << "begin_frame";
    vkd.vkdf->vkWaitForFences(vkd.device, 1, &frame_resource.fence, VK_TRUE, -1);

    res = vkAcquireNextImageKHR(vkd.device, swap_chain, -1, frame_resources[frame_index].image_available_semaphore, VK_NULL_HANDLE, &image_index);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) // Resize will be dealt with by `resizeEvent`
        return;
    else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
        qFatal("VulkanWindow: Failed to aquire next image to render on: %d", res);

    ImageResources& image_resource = image_resources[image_index];

    // Make sure previous images & frames have finished using their resources
    if (image_resource.fence != VK_NULL_HANDLE) {
        vkd.vkdf->vkWaitForFences(vkd.device, 1, &image_resource.fence, VK_TRUE, -1);
    }
    image_resource.fence = frame_resource.fence;

    vkd.vkdf->vkFreeCommandBuffers(vkd.device, command_pool, 1, &image_resource.command_buffer);
    create_command_buffer(image_resource.command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;

    res = vkd.vkdf->vkBeginCommandBuffer(image_resource.command_buffer, &begin_info);
    if (res != VK_SUCCESS)
        qFatal("VulkanWindow: Failed to begin recording framebuffer: %d", res);

    vulkan_renderer->start_next_frame();
}

void VulkanWindow::end_frame() {
    ImageResources& image_resource = image_resources[image_index];
    FrameResources& frame_resource = frame_resources[frame_index];
    VkResult res;

    res = vkd.vkdf->vkEndCommandBuffer(image_resource.command_buffer);
    if (res != VK_SUCCESS)
        qFatal("VulkanWindow: Failed to end recording framebuffer: %d", res);

    vkd.vkdf->vkResetFences(vkd.device, 1, &frame_resource.fence);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &frame_resource.image_available_semaphore;
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &image_resource.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &frame_resource.render_finished_semaphore;

    res = vkd.vkdf->vkQueueSubmit(graphics_queue, 1, &submit_info, frame_resource.fence);
    if (res != VK_SUCCESS)
        qFatal("VulkanWindow: Failed to submit queue: %d", res);

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &frame_resource.render_finished_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swap_chain;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    res = vkQueuePresentKHR(present_queue, &present_info);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) // Resize will be dealt with by `resizeEvent`
        return;
    else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
        qFatal("VulkanWindow: Failed to present queue: %d", res);
    vulkanInstance()->presentQueued(this);

    frame_index = (frame_index + 1) % nr_frames_in_flight;
    // requestUpdate();
}

void VulkanWindow::resolve_instance_extension_functions() {
    QVulkanInstance* instance = vulkanInstance();
    vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        instance->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceSupportKHR")
    );
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        instance->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceCapabilitiesKHR")
    );
    vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
        instance->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceFormatsKHR")
    );
    vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
        instance->getInstanceProcAddr("vkGetPhysicalDeviceSurfacePresentModesKHR")
    );
}

void VulkanWindow::resolve_device_extension_functions() {
    vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
        vkd.vkf->vkGetDeviceProcAddr(vkd.device, "vkCreateSwapchainKHR")
    );
    vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
        vkd.vkf->vkGetDeviceProcAddr(vkd.device, "vkDestroySwapchainKHR")
    );
    vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
        vkd.vkf->vkGetDeviceProcAddr(vkd.device, "vkGetSwapchainImagesKHR")
    );
    vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
        vkd.vkf->vkGetDeviceProcAddr(vkd.device, "vkAcquireNextImageKHR")
    );
    vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
        vkd.vkf->vkGetDeviceProcAddr(vkd.device, "vkQueuePresentKHR")
    );
}


void VulkanWindow::exposeEvent(QExposeEvent*) {
    if (isExposed()) {
        if (status == VulkanWindow::Status::Uninitialized) {
            init_resources();
        }
        if (status == VulkanWindow::Status::Device_Ready) {
            init_swap_chain_resources();
        }
        if (status == VulkanWindow::Status::Ready) {
            requestUpdate();
        }
    }
    else {
        release_swap_chain_resources();
        release_resources();
    }
}

void VulkanWindow::resizeEvent(QResizeEvent*) {
    if (status == Status::Ready) {
        release_swap_chain_resources();
        init_swap_chain_resources();
    }
}

bool VulkanWindow::event(QEvent* event) {
    switch (event->type()) {
    case QEvent::UpdateRequest:
        begin_frame();
        break;
    case QEvent::PlatformSurface:
        if (static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            release_swap_chain_resources();
            release_resources();
        }
        break;
    default:
        break;
    }
    return QWindow::event(event);
}


void VulkanWindow::create_surface() {
    surface = QVulkanInstance::surfaceForWindow(this);
    if (surface == VK_NULL_HANDLE)
        qFatal("VulkanWindow: Failed to retrieve Vulkan surface for window");
}

VulkanWindow::SwapChainSupportDetails VulkanWindow::query_swap_chain_support_details(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    details.formats.resize(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
    
    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
    details.present_modes.resize(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());

    return details;
}

VulkanWindow::QueueFamilyIndices VulkanWindow::find_queue_families(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queue_family_count = 0;
    vkd.vkf->vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
    vkd.vkf->vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_family_properties.data());


    for (uint32_t i=0; i<queue_family_properties.size(); i++) {
        if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics_family = i;
        
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support)
            indices.present_family = i;
    }

    return indices;
}

bool VulkanWindow::check_device_extension_support(VkPhysicalDevice device) {
    uint32_t extension_count;
    vkd.vkf->vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkd.vkf->vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());

    size_t nr_unsupported_extensions = required_extensions.size();
    for (const auto& extension : available_extensions) {
        if (required_extensions.find(extension.extensionName) != required_extensions.end()) 
            nr_unsupported_extensions -= 1;
    }
    return nr_unsupported_extensions == 0;
}

int VulkanWindow::rate_device_suitability(VkPhysicalDevice device) {
    int score = 1;

    if (!find_queue_families(device).is_complete())
        return 0;

    if (!check_device_extension_support(device))
        return 0;

    SwapChainSupportDetails swap_chain_support_details = query_swap_chain_support_details(device);
    if (swap_chain_support_details.formats.empty() && swap_chain_support_details.present_modes.empty())
        return 0;

    if (physical_device_rater != nullptr)
        return physical_device_rater(device, score);

    return score;
}

void VulkanWindow::pick_physical_device() {
    uint32_t device_count = 0;
    vkd.vkf->vkEnumeratePhysicalDevices(vulkanInstance()->vkInstance(), &device_count, nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkd.vkf->vkEnumeratePhysicalDevices(vulkanInstance()->vkInstance(), &device_count, devices.data());

    int best_score = 0;
    for (const auto& device : devices) {
        if (rate_device_suitability(device) > best_score) {
            vkd.physical_device = device;
        }
    }

    if (vkd.physical_device == VK_NULL_HANDLE)
        qFatal("VulkanWindow: Unable to find suitable GPU");
}

void VulkanWindow::create_logical_device() {
    queue_families = find_queue_families(vkd.physical_device);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

    std::set<uint32_t> unique_queue_families = {
        queue_families.graphics_family.value(),
        queue_families.present_family.value()
    };

    float queue_priority = 1.0f;
    for (uint32_t queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;

        queue_create_infos.push_back(queue_create_info);
    }

    physical_device_features = requested_physical_device_features;
    VkPhysicalDeviceFeatures supported_device_features;
    vkd.vkf->vkGetPhysicalDeviceFeatures(vkd.physical_device, &supported_device_features);

    // Super unsafe but the only way I know to disable unsupported features without hardcoding an if for each feature
    // Hopefully messing up whatever padding the compiler puts (if the compiler does put padding) doesn't cause errors
    unsigned char* supported_device_features_memory = reinterpret_cast<unsigned char*>(&supported_device_features);
    unsigned char* device_features_memory = reinterpret_cast<unsigned char*>(&physical_device_features);
    if (sizeof(VkPhysicalDeviceFeatures) != 55*4)
        qWarning("VulkanWndow: Unexpected VkPhysicalDeviceFeatures size. This might be because more features were added or because padding was added by the compiler.");
    for (size_t i=0; i<sizeof(VkPhysicalDeviceFeatures); i++) {
        device_features_memory[i] &= supported_device_features_memory[i];
    }

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = queue_create_infos.size();
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &physical_device_features;

    create_info.enabledExtensionCount = device_extensions.size();
    create_info.ppEnabledExtensionNames = device_extensions.data();

    // Device layers deprecated
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = nullptr;

    VkResult res = vkd.vkf->vkCreateDevice(vkd.physical_device, &create_info, nullptr, &vkd.device);
    if (res != VK_SUCCESS)
        qFatal("VulkanWindow: Failed to create logical device: %d", res);
}

void VulkanWindow::create_queues() {
    vkd.vkdf->vkGetDeviceQueue(vkd.device, queue_families.graphics_family.value(), 0, &graphics_queue);
    vkd.vkdf->vkGetDeviceQueue(vkd.device, queue_families.present_family.value(), 0, &present_queue);
}

void VulkanWindow::create_command_pool() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_families.graphics_family.value();
    pool_info.flags = 0;

    VkResult res = vkd.vkdf->vkCreateCommandPool(vkd.device, &pool_info, nullptr, &command_pool);
    if (res != VK_SUCCESS)
        qFatal("Failed to create command pool: %d", res);
}

VkSurfaceFormatKHR VulkanWindow::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) {
    for (const auto& available_format : available_formats) {
        if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_format;
        }
    }
    return available_formats[0];
}

int VulkanWindow::find_supported_format(const VkFormat* formats, int nr_candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (int i=0; i<nr_candidates; i++) {
        VkFormatProperties properties;
        vkd.vkf->vkGetPhysicalDeviceFormatProperties(vkd.physical_device, formats[i], &properties);
        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) {
            return i;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features) {
            return i;
        }
    }
    return -1;
}

VkFormat VulkanWindow::find_depth_format() {
    VkFormat depth_formats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    int index = find_supported_format(
        depth_formats,
        sizeof(depth_formats)/sizeof(depth_formats[0]),
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    if (index == -1) qFatal("Failed to find supported depth format.");
    return depth_formats[index];
}

bool VulkanWindow::has_stencil_component(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanWindow::create_default_render_pass() {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swap_chain_surface_format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_reference{};
    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_stencil_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_reference{};
    depth_attachment_reference.attachment = 1;
    depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;
    subpass.pDepthStencilAttachment = &depth_attachment_reference;

    VkSubpassDependency subpass_dependency{};
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.dstSubpass = 0;
    subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.srcAccessMask = 0;
    subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {
        color_attachment,
        depth_attachment,
    };

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = sizeof(attachments)/sizeof(attachments[0]);
    render_pass_create_info.pAttachments = attachments;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &subpass_dependency;

    VkResult res = vkd.vkdf->vkCreateRenderPass(vkd.device, &render_pass_create_info, nullptr, &default_render_pass);
    if (res != VK_SUCCESS)
        qFatal("VulkanWindow: Failed to create default render pass: %d", res);
}

VkPresentModeKHR VulkanWindow::choose_swap_present_mode(const std::vector<VkPresentModeKHR> available_present_modes) {
    for (const auto& available_present_mode : available_present_modes) {
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return available_present_mode;
    }
    // Guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanWindow::get_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) {
    // Special case where the surface size will be determined by the extent of a swapchain targeting the surface
    if (capabilities.currentExtent.width == 0xFFFFFFFF) {
        return capabilities.currentExtent;
    }

    VkExtent2D actual_extent = {
        std::max(capabilities.minImageExtent.width, std::min((uint32_t) width(), capabilities.maxImageExtent.width)),
        std::max(capabilities.minImageExtent.height, std::min((uint32_t) height(), capabilities.maxImageExtent.height)),
    };
    return actual_extent;
}

void VulkanWindow::create_swap_chain() {
    VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support_details.present_modes);
    swap_chain_extent = get_swap_extent(swap_chain_support_details.capabilities);

    image_count = std::max(image_count, swap_chain_support_details.capabilities.minImageCount);
    if (swap_chain_support_details.capabilities.maxImageCount != 0)
        image_count = std::min(image_count, swap_chain_support_details.capabilities.maxImageCount);
    
    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = swap_chain_surface_format.format;
    create_info.imageColorSpace = swap_chain_surface_format.colorSpace;
    create_info.imageExtent = swap_chain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queue_families_array[] = {
        queue_families.graphics_family.value(),
        queue_families.present_family.value()
    };
    if (queue_families.graphics_family.value() == queue_families.present_family.value()) {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // queueFamilyIndexCount and pQueueFamilyIndices are only needed with VK_SHARING_MODE_CONCURRENT
    }
    else {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_families_array;
    }

    create_info.preTransform = swap_chain_support_details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult res = vkCreateSwapchainKHR(vkd.device, &create_info, nullptr, &swap_chain);
    if (res != VK_SUCCESS)
        qFatal("VulkanWindow: Failed to create swapchain: %d", res);
}

void VulkanWindow::create_depth_image() {
    Image::CreateData icd{};
    icd.width = swap_chain_extent.width;
    icd.height = swap_chain_extent.height;
    icd.format = depth_stencil_format;
    icd.tiling = VK_IMAGE_TILING_OPTIMAL;
    icd.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    icd.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    icd.aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (has_stencil) 
        icd.aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

    depth_image.create(vkd, icd);
    depth_image.create_view(VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanWindow::get_swap_chain_images() {
    vkGetSwapchainImagesKHR(vkd.device, swap_chain, &image_count, nullptr);
    std::vector<VkImage> swap_chain_images(image_count);
    vkGetSwapchainImagesKHR(vkd.device, swap_chain, &image_count, swap_chain_images.data());

    image_resources.resize(image_count);
    for (size_t i=0; i<swap_chain_images.size(); i++) {
        image_resources[i].image = swap_chain_images[i];
        // Also get the image view
        VkResult res = Image::create_view(vkd, image_resources[i].image, swap_chain_surface_format.format, VK_IMAGE_ASPECT_COLOR_BIT, image_resources[i].image_view);
        if (res != VK_SUCCESS)
            qFatal("Failed to create image view: %d", res);
    }
}

void VulkanWindow::create_frame_buffers() {
    for (auto& image_resource : image_resources) {
        VkImageView attachments[] = {image_resource.image_view, depth_image.get_vk_image_view()};

        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = default_render_pass;
        framebuffer_create_info.attachmentCount = sizeof(attachments)/sizeof(attachments[0]);
        framebuffer_create_info.pAttachments = attachments;
        framebuffer_create_info.width = swap_chain_extent.width;
        framebuffer_create_info.height = swap_chain_extent.height;
        framebuffer_create_info.layers = 1;

        VkResult res = vkd.vkdf->vkCreateFramebuffer(vkd.device, &framebuffer_create_info, nullptr, &image_resource.framebuffer);
        if (res != VK_SUCCESS)
            qFatal("VulkanWindow: Failed to create framebuffer: %d", res);
    }
}

void VulkanWindow::create_sync_objects() {
    VkResult res;

    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& frame_resource : frame_resources) {
        res = vkd.vkdf->vkCreateSemaphore(vkd.device, &semaphore_create_info, nullptr, &frame_resource.image_available_semaphore);
        if (res != VK_SUCCESS) 
            qFatal("VulkanWindow: Failed to create semaphore: %d", res);

        res = vkd.vkdf->vkCreateSemaphore(vkd.device, &semaphore_create_info, nullptr, &frame_resource.render_finished_semaphore);
        if (res != VK_SUCCESS) 
            qFatal("VulkanWindow: Failed to create semaphore: %d", res);

        res = vkd.vkdf->vkCreateFence(vkd.device, &fence_create_info, nullptr, &frame_resource.fence);
        if (res != VK_SUCCESS)
            qFatal("VulkanWindow: Failed to create fence: %d", res);
    }
}

void VulkanWindow::create_command_buffer(VkCommandBuffer& command_buffer) {
    VkCommandBufferAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation_info.commandPool = command_pool;
    allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation_info.commandBufferCount = 1;

    VkResult res = vkd.vkdf->vkAllocateCommandBuffers(vkd.device, &allocation_info, &command_buffer);
    if (res != VK_SUCCESS)
        qFatal("VulkanWindow: Failed to allocate command buffer: %d", res);
}