#include "VulkanRenderer.hpp"

#include <QVulkanDeviceFunctions>
#include <QFile>
#include <QString>
#include <QDebug>

#include "Shader.hpp"
#include "Vertex.hpp"

const std::vector<Vertex> vertices = {
    Vertex{glm::vec2(-0.5f, -0.5f), glm::vec3(1.0f,0.0f,0.0f), glm::vec2(0.0f,0.0f)},
    Vertex{glm::vec2( 0.5f, -0.5f), glm::vec3(0.0f,1.0f,0.0f), glm::vec2(1.0f,0.0f)},
    Vertex{glm::vec2( 0.5f,  0.5f), glm::vec3(0.0f,0.0f,1.0f), glm::vec2(1.0f,1.0f)},
    Vertex{glm::vec2(-0.5f,  0.5f), glm::vec3(1.0f,1.0f,1.0f), glm::vec2(0.0f,1.0f)},
};

const std::vector<Index> indices = {
    0, 1, 2,
    2, 3, 0,
};

VulkanRenderer::VulkanRenderer() {}

VulkanRenderer::~VulkanRenderer() {}

void VulkanRenderer::pre_init_resources(VulkanWindow* vulkan_window) {
    fps_timer.start();

    qDebug() << "pre_init_resources";
    this->vulkan_window = vulkan_window;

    QVulkanFunctions* vkf = vulkan_window->vulkanInstance()->functions();
        
    vulkan_window->set_physical_device_rater(
        [vkf](VkPhysicalDevice phys_dev, int window_score){
            VkPhysicalDeviceFeatures device_features;
            vkf->vkGetPhysicalDeviceFeatures(phys_dev, &device_features);
            if (device_features.samplerAnisotropy)
                window_score += 10;
            return window_score;
        }
    );

    VkPhysicalDeviceFeatures requested_physical_device_features{};
    requested_physical_device_features.samplerAnisotropy = VK_TRUE;
    vulkan_window->request_physical_device_features(requested_physical_device_features);
}

void VulkanRenderer::init_resources() {
    qDebug() << "init_resources";
    vkd = vulkan_window->get_vulkan_data();
    enabled_device_features = vulkan_window->get_enabled_physical_device_features();

    create_descriptor_set_layout();
    create_graphics_pipeline();
    create_vertex_buffer();
    create_index_buffer();
    create_texture_image();
}

void VulkanRenderer::init_swap_chain_resources() {
    qDebug() << "init_swap_chain_resources";

    // Shouldn't change but just in case
    frame_resources.resize(vulkan_window->get_nr_concurrent_frames());
    create_descriptor_pool();
    create_uniform_buffers();
    create_descriptor_sets();

    control_panel.show();
}

void VulkanRenderer::release_swap_chain_resources() {
    qDebug() << "release_swap_chain_resources";
    control_panel.hide();

    for (auto& frame_resource : frame_resources) {
        vkd.vkdf->vkDestroyBuffer(vkd.device, frame_resource.uniform_buffer, nullptr);
        frame_resource.uniform_buffer = VK_NULL_HANDLE;
        vkd.vkdf->vkFreeMemory(vkd.device, frame_resource.uniform_buffer_memory, nullptr);
        frame_resource.uniform_buffer_memory = VK_NULL_HANDLE;
    }

    vkd.vkdf->vkDestroyDescriptorPool(vkd.device, descriptor_pool, nullptr);
    descriptor_pool = VK_NULL_HANDLE;
}

void VulkanRenderer::release_resources() {
    qDebug() << "release_resources";

    texture_image.destroy();

    vkd.vkdf->vkDestroyBuffer(vkd.device, vertex_buffer, nullptr);
    vertex_buffer = VK_NULL_HANDLE;
    vkd.vkdf->vkFreeMemory(vkd.device, vertex_buffer_memory, nullptr);
    vertex_buffer_memory = VK_NULL_HANDLE;

    vkd.vkdf->vkDestroyBuffer(vkd.device, index_buffer, nullptr);
    index_buffer = VK_NULL_HANDLE;
    vkd.vkdf->vkFreeMemory(vkd.device, index_buffer_memory, nullptr);
    index_buffer_memory = VK_NULL_HANDLE;

    vkd.vkdf->vkDestroyDescriptorSetLayout(vkd.device, descriptor_set_layout, nullptr);
    descriptor_set_layout = VK_NULL_HANDLE;

    vkd.vkdf->vkDestroyPipeline(vkd.device, graphics_pipeline, nullptr);
    graphics_pipeline = VK_NULL_HANDLE;
    vkd.vkdf->vkDestroyPipelineLayout(vkd.device, pipeline_layout, nullptr);
    pipeline_layout = VK_NULL_HANDLE;

    vkd.physical_device = VK_NULL_HANDLE;
    vkd.device = VK_NULL_HANDLE;
    vkd.vkf = nullptr;
    vkd.vkdf = nullptr;
}

void VulkanRenderer::start_next_frame() {
    int frame_time = fps_timer.elapsed();
    control_panel.update_frame_time(frame_time);
    fps_timer.start();

    VkCommandBuffer command_buffer = vulkan_window->get_current_command_buffer();

    uint32_t current_frame_index = vulkan_window->get_current_frame_index();
    update_uniform_buffer(current_frame_index);

    static float green = 0.0f;
    green += 0.005f;
    if (green > 1.0f) green -= 1.0f;
    VkClearColorValue clear_color = {{0.0f, green, 0.0f, 1.0f}};
    VkClearValue clear_value{};
    clear_value.color = clear_color;

    VkExtent2D extent = vulkan_window->get_image_extent();

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = extent.width;
    viewport.height = extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    vkd.vkdf->vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkd.vkdf->vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = vulkan_window->get_default_render_pass();
    render_pass_begin_info.framebuffer = vulkan_window->get_current_frame_buffer();
    render_pass_begin_info.renderArea.extent = vulkan_window->get_image_extent();
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;

    vkd.vkdf->vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkd.vkdf->vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

    VkDeviceSize offsets[] = {0};
    vkd.vkdf->vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vkd.vkdf->vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, offsets);

    vkd.vkdf->vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &frame_resources[current_frame_index].descriptor_set, 0, nullptr);
    vkd.vkdf->vkCmdDrawIndexed(command_buffer, indices.size(), 1, 0, 0, 0);
    vkd.vkdf->vkCmdEndRenderPass(command_buffer);

    vulkan_window->frame_ready();
    vulkan_window->requestUpdate();
}

void VulkanRenderer::create_descriptor_set_layout() {
    VkDescriptorSetLayoutBinding ubo_layout_binding{};
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    ubo_layout_binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding sampler_layout_binding{};
    sampler_layout_binding.binding = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    sampler_layout_binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding bindings[] = {
        ubo_layout_binding,
        sampler_layout_binding,
    };

    VkDescriptorSetLayoutCreateInfo layout_create_info{};
    layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_create_info.bindingCount = sizeof(bindings)/sizeof(bindings[0]);
    layout_create_info.pBindings = bindings;

    VkResult res = vkd.vkdf->vkCreateDescriptorSetLayout(vkd.device, &layout_create_info, nullptr, &descriptor_set_layout);
    if (res != VK_SUCCESS)
        qFatal("Failed to create descriptor set layout: %d", res);
}

void VulkanRenderer::create_graphics_pipeline() {
    VkResult res;

    ShaderModule vertex_shader_module(vkd.device, vkd.vkdf, "src/shaders/vert.spv");
    ShaderModule fragment_shader_module(vkd.device, vkd.vkdf, "src/shaders/frag.spv");

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vertex_shader_module.get_create_info(VK_SHADER_STAGE_VERTEX_BIT),
        fragment_shader_module.get_create_info(VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    auto binding_description = Vertex::get_binding_description();
    auto attribute_descriptions = Vertex::get_attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = attribute_descriptions.size();
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    // Viewport & Scissor are dynamically set at runtime
    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization_info{};
    rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info.depthClampEnable = VK_FALSE;
    rasterization_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_info.lineWidth = 1.0f;
    rasterization_info.cullMode = VK_CULL_MODE_NONE; //VK_CULL_MODE_BACK_BIT;
    rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_info.depthBiasEnable = VK_FALSE;
    rasterization_info.depthBiasConstantFactor = 0.0f;
    rasterization_info.depthBiasClamp = 0.0f;
    rasterization_info.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling_info{};
    multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_info.sampleShadingEnable = VK_FALSE;
    multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling_info.minSampleShading = 1.0f;
    multisampling_info.pSampleMask = nullptr;
    multisampling_info.alphaToCoverageEnable = VK_FALSE;
    multisampling_info.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blending_info{};
    color_blending_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending_info.logicOpEnable = VK_FALSE;
    color_blending_info.attachmentCount = 1;
    color_blending_info.pAttachments = &color_blend_attachment;

    VkDynamicState enabled_dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state_info{};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = sizeof(enabled_dynamic_states) / sizeof(enabled_dynamic_states[0]);
    dynamic_state_info.pDynamicStates = enabled_dynamic_states;
    
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;

    res = vkd.vkdf->vkCreatePipelineLayout(vkd.device, &pipeline_layout_info, nullptr, &pipeline_layout);
    if (res != VK_SUCCESS)
        qFatal("Failed to create pipeline layout: %d", res);

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterization_info;
    pipeline_info.pMultisampleState = &multisampling_info;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending_info;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = vulkan_window->get_default_render_pass();
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    res = vkd.vkdf->vkCreateGraphicsPipelines(vkd.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline);
    if (res != VK_SUCCESS)
        qFatal("Failed to create graphics pipeline: %d", res);
}

void VulkanRenderer::create_vertex_buffer() {
    VkDeviceSize buffer_size = vertices.size() * sizeof(Vertex);

    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_buffer, vertex_buffer_memory);
    copy_data_to_buffer(vertex_buffer, vertices.data(), buffer_size);
}

void VulkanRenderer::create_index_buffer() {
    VkDeviceSize buffer_size = indices.size() * sizeof(Index);

    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_buffer, index_buffer_memory);
    copy_data_to_buffer(index_buffer, indices.data(), buffer_size);
}

void VulkanRenderer::create_texture_image() {
    const QImage qt_image = QImage("textures/awesomeface.png").convertToFormat(QImage::Format_RGBA8888);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    create_buffer(qt_image.sizeInBytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

    void* data;
    vkd.vkdf->vkMapMemory(vkd.device, staging_buffer_memory, 0, qt_image.sizeInBytes(), 0, &data);
    memcpy(data, qt_image.constBits(), qt_image.sizeInBytes());
    vkd.vkdf->vkUnmapMemory(vkd.device, staging_buffer_memory);

    Image::CreateData icd = Image::CreateData::default_texture_data(qt_image.width(), qt_image.height());
    icd.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    texture_image.create(vkd, icd);

    VkCommandPool command_pool = vulkan_window->get_graphics_command_pool();
    VkCommandBuffer command_buffer = begin_single_time_commands(vkd, command_pool);

    texture_image.transition_image_layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, command_buffer);
    texture_image.copy_buffer_to_image(staging_buffer, command_buffer);
    texture_image.transition_image_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, command_buffer);

    VkQueue queue = vulkan_window->get_graphics_queue();
    end_single_time_commands(vkd, command_pool, queue, command_buffer, fence_timeout);

    texture_image.create_view();
    VkSamplerCreateInfo sci = Image::default_texture_sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, enabled_device_features.samplerAnisotropy);
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    texture_image.create_sampler(sci);

    vkd.vkdf->vkDestroyBuffer(vkd.device, staging_buffer, nullptr);
    vkd.vkdf->vkFreeMemory(vkd.device, staging_buffer_memory, nullptr);
}

void VulkanRenderer::create_descriptor_pool() {
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (uint32_t)frame_resources.size()},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)frame_resources.size()},
    };

    VkDescriptorPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.poolSizeCount = sizeof(pool_sizes)/sizeof(pool_sizes[0]);
    pool_create_info.pPoolSizes = pool_sizes;
    pool_create_info.maxSets = frame_resources.size();


    VkResult res = vkd.vkdf->vkCreateDescriptorPool(vkd.device, &pool_create_info, nullptr, &descriptor_pool);
    if (res != VK_SUCCESS)
        qFatal("Failed to create descriptor pool: %d", res);
}

void VulkanRenderer::create_uniform_buffers() {
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    for (auto& frame_resource : frame_resources) {
        create_buffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, frame_resource.uniform_buffer, frame_resource.uniform_buffer_memory);
    }
}

void VulkanRenderer::create_descriptor_sets() {
    VkDescriptorSetAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocation_info.descriptorPool = descriptor_pool;
    allocation_info.descriptorSetCount = 1;
    allocation_info.pSetLayouts = &descriptor_set_layout;

    for (auto& frame_resource : frame_resources) {
        VkResult res = vkd.vkdf->vkAllocateDescriptorSets(vkd.device, &allocation_info, &frame_resource.descriptor_set);
        if (res != VK_SUCCESS)
            qFatal("Failed to alocate descriptor set: %d", res);

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = frame_resource.uniform_buffer;
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = texture_image.get_vk_image_view();
        image_info.sampler = texture_image.get_vk_sampler();

        VkWriteDescriptorSet descriptor_writes[2] = {};

        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = frame_resource.descriptor_set;
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].pBufferInfo = &buffer_info;

        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet = frame_resource.descriptor_set;
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].pImageInfo = &image_info;

        vkd.vkdf->vkUpdateDescriptorSets(vkd.device, sizeof(descriptor_writes)/sizeof(descriptor_writes[0]), descriptor_writes, 0, nullptr);
    }
}

void VulkanRenderer::update_uniform_buffer(uint32_t current_frame_index) {
    static float angle = 0.0f;
    angle += 0.025f;
    ubo.model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f,1.0f,0.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f,2.0f,2.0f), glm::vec3(0.0f,0.0f,0.0f), glm::vec3(0.0f,1.0f,0.0f));
    ubo.projection = glm::perspective(glm::radians(45.0f), vulkan_window->width()/float(vulkan_window->height()), 0.1f, 10.0f);

    void* data;
    vkd.vkdf->vkMapMemory(vkd.device, frame_resources[current_frame_index].uniform_buffer_memory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkd.vkdf->vkUnmapMemory(vkd.device, frame_resources[current_frame_index].uniform_buffer_memory);
}

void VulkanRenderer::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_properties, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkd.vkdf->vkCreateBuffer(vkd.device, &buffer_info, nullptr, &buffer);
    if (res != VK_SUCCESS) {
        qFatal("Failed to allocate memory for buffer: %d", res);
    }

    VkMemoryRequirements memory_requirements;
    vkd.vkdf->vkGetBufferMemoryRequirements(vkd.device, buffer, &memory_requirements);

    VkMemoryAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = find_memory_type(vkd, memory_requirements.memoryTypeBits, memory_properties);
    if (allocation_info.memoryTypeIndex == uint32_t(-1))
        qFatal("Failed to find suitable memory type index.");

    res = vkd.vkdf->vkAllocateMemory(vkd.device, &allocation_info, nullptr, &memory);
    if (res != VK_SUCCESS) {
        qFatal("Failed to allocate memory for vertex buffer: %d", res);
    }

    vkd.vkdf->vkBindBufferMemory(vkd.device, buffer, memory, 0);
}

void VulkanRenderer::copy_data_to_buffer(VkBuffer dst_buffer, const void* data, VkDeviceSize size) {
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);
    void* buffer_data;
    vkd.vkdf->vkMapMemory(vkd.device, staging_buffer_memory, 0, size, 0, &buffer_data);
    memcpy(buffer_data, data, size);
    vkd.vkdf->vkUnmapMemory(vkd.device, staging_buffer_memory);

    copy_buffer(staging_buffer, dst_buffer, size);

    vkd.vkdf->vkDestroyBuffer(vkd.device, staging_buffer, nullptr);
    vkd.vkdf->vkFreeMemory(vkd.device, staging_buffer_memory, nullptr);
}

void VulkanRenderer::copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
    VkCommandBuffer command_buffer = begin_single_time_commands(vkd, vulkan_window->get_graphics_command_pool());

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;
    vkd.vkdf->vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    end_single_time_commands(vkd, vulkan_window->get_graphics_command_pool(), vulkan_window->get_graphics_queue(), command_buffer, fence_timeout);
}
