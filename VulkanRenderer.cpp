#include "VulkanRenderer.hpp"

#include <QVulkanDeviceFunctions>
#include <QFile>

#include "Shader.hpp"
#include "Vertex.hpp"

const std::vector<Vertex> vertices = {
    Vertex{glm::vec2(-0.5f, -0.5f), glm::vec3(1.0f,0.0f,0.0f)},
    Vertex{glm::vec2( 0.5f, -0.5f), glm::vec3(0.0f,1.0f,0.0f)},
    Vertex{glm::vec2( 0.5f,  0.5f), glm::vec3(0.0f,0.0f,1.0f)},
    Vertex{glm::vec2(-0.5f,  0.5f), glm::vec3(1.0f,1.0f,1.0f)},
};

const std::vector<Index> indices = {
    0, 1, 2,
    2, 3, 0,
};

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

    create_descriptor_set_layout();
    create_graphics_pipeline();
    create_vertex_buffer();
    create_index_buffer();
}

void VulkanRenderer::init_swap_chain_resources() {
    qDebug() << "init_swap_chain_resources";

    // Shouldn't change but just in case
    frame_resources.resize(vulkan_window->get_nr_concurrent_frames());
    create_descriptor_pool();
    create_uniform_buffers();
    create_descriptor_sets();
}

void VulkanRenderer::release_swap_chain_resources() {
    qDebug() << "release_swap_chain_resources";
    for (auto& frame_resource : frame_resources) {
        vkdf->vkDestroyBuffer(device, frame_resource.uniform_buffer, nullptr);
        frame_resource.uniform_buffer = VK_NULL_HANDLE;
        vkdf->vkFreeMemory(device, frame_resource.uniform_buffer_memory, nullptr);
        frame_resource.uniform_buffer_memory = VK_NULL_HANDLE;
    }

    vkdf->vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    descriptor_pool = VK_NULL_HANDLE;
}

void VulkanRenderer::release_resources() {
    qDebug() << "release_resources";

    vkdf->vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
    descriptor_set_layout = VK_NULL_HANDLE;

    vkdf->vkDestroyBuffer(device, vertex_buffer, nullptr);
    vertex_buffer = VK_NULL_HANDLE;
    vkdf->vkFreeMemory(device, vertex_buffer_memory, nullptr);
    vertex_buffer_memory = VK_NULL_HANDLE;

    vkdf->vkDestroyBuffer(device, index_buffer, nullptr);
    index_buffer = VK_NULL_HANDLE;
    vkdf->vkFreeMemory(device, index_buffer_memory, nullptr);
    index_buffer_memory = VK_NULL_HANDLE;

    vkdf->vkDestroyPipeline(device, graphics_pipeline, nullptr);
    graphics_pipeline = VK_NULL_HANDLE;
    vkdf->vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    pipeline_layout = VK_NULL_HANDLE;

    device = VK_NULL_HANDLE;
    vkf = nullptr;
    vkdf = nullptr;
}

void VulkanRenderer::start_next_frame() {
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

    vkdf->vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkdf->vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = vulkan_window->get_default_render_pass();
    render_pass_begin_info.framebuffer = vulkan_window->get_current_frame_buffer();
    render_pass_begin_info.renderArea.extent = vulkan_window->get_image_extent();
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;

    vkdf->vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkdf->vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

    VkDeviceSize offsets[] = {0};
    vkdf->vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vkdf->vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, offsets);

    vkdf->vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &frame_resources[current_frame_index].descriptor_set, 0, nullptr);
    vkdf->vkCmdDrawIndexed(command_buffer, indices.size(), 1, 0, 0, 0);
    vkdf->vkCmdEndRenderPass(command_buffer);

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

    VkDescriptorSetLayoutCreateInfo layout_create_info{};
    layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_create_info.bindingCount = 1;
    layout_create_info.pBindings = &ubo_layout_binding;

    VkResult res = vkdf->vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layout);
    if (res != VK_SUCCESS)
        qFatal("Failed to create descriptor set layout: %d", res);
}

void VulkanRenderer::create_graphics_pipeline() {
    VkResult res;

    ShaderModule vertex_shader_module(device, vkdf, "shaders/vert.spv");
    ShaderModule fragment_shader_module(device, vkdf, "shaders/frag.spv");

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

    res = vkdf->vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout);
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

    res = vkdf->vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline);
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

void VulkanRenderer::create_descriptor_pool() {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = frame_resources.size();

    VkDescriptorPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.poolSizeCount = 1;
    pool_create_info.pPoolSizes = &pool_size;
    pool_create_info.maxSets = frame_resources.size();

    VkResult res = vkdf->vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool);
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

    // std::vector<VkDescriptorSet> descriptor_sets(frame_resources.size());
    // VkResult res = vkdf->vkAllocateDescriptorSets(device, &allocation_info, descriptor_sets.data());
    // if (res != VK_SUCCESS)
    //     qFatal();

    for (auto& frame_resource : frame_resources) {
        VkResult res = vkdf->vkAllocateDescriptorSets(device, &allocation_info, &frame_resource.descriptor_set);
        if (res != VK_SUCCESS)
            qFatal("Failed to alocate descriptor set: %d", res);

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = frame_resource.uniform_buffer;
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = frame_resource.descriptor_set;
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &buffer_info;
        descriptor_write.pImageInfo = nullptr;
        descriptor_write.pTexelBufferView = nullptr;

        vkdf->vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, nullptr);
    }
}

void VulkanRenderer::update_uniform_buffer(uint32_t current_frame_index) {
    static float angle = 1.0f;
    angle += 0.025f;
    ubo.model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f,1.0f,0.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f,2.0f,2.0f), glm::vec3(0.0f,0.0f,0.0f), glm::vec3(0.0f,1.0f,0.0f));
    ubo.projection = glm::perspective(glm::radians(45.0f), vulkan_window->width()/float(vulkan_window->height()), 0.1f, 10.0f);

    void* data;
    vkdf->vkMapMemory(device, frame_resources[current_frame_index].uniform_buffer_memory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkdf->vkUnmapMemory(device, frame_resources[current_frame_index].uniform_buffer_memory);
}

VkCommandBuffer VulkanRenderer::begin_single_time_commands() {
    VkCommandBufferAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation_info.commandPool = vulkan_window->get_graphics_command_pool();
    allocation_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkdf->vkAllocateCommandBuffers(device, &allocation_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkdf->vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void VulkanRenderer::end_single_time_commands(VkCommandBuffer command_buffer) {
    vkdf->vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    vkdf->vkCreateFence(device, &fence_create_info, nullptr, &fence);

    vkdf->vkQueueSubmit(vulkan_window->get_graphics_queue(), 1, &submit_info, fence);

    VkResult res = vkdf->vkWaitForFences(device, 1, &fence, VK_FALSE, fence_timeout);
    if (res != VK_SUCCESS)
        qWarning("Timeout waiting for fence: %d", res);
    
    vkdf->vkFreeCommandBuffers(device, vulkan_window->get_graphics_command_pool(), 1, &command_buffer);
    vkdf->vkDestroyFence(device, fence, nullptr);
}

uint32_t VulkanRenderer::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkf->vkGetPhysicalDeviceMemoryProperties(vulkan_window->get_physical_device(), &memory_properties);
    for (uint32_t i=0; i<memory_properties.memoryTypeCount; i++) {
        if (type_filter & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    qFatal("Failed to find suitable memory type");
}

void VulkanRenderer::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_properties, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkdf->vkCreateBuffer(device, &buffer_info, nullptr, &buffer);
    if (res != VK_SUCCESS) {
        qFatal("Failed to allocate memory for vertex buffer: %d", res);
    }

    VkMemoryRequirements memory_requirements;
    vkdf->vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

    VkMemoryAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, memory_properties);

    res = vkdf->vkAllocateMemory(device, &allocation_info, nullptr, &memory);
    if (res != VK_SUCCESS) {
        qFatal("Failed to allocate memory for vertex buffer: %d", res);
    }

    vkdf->vkBindBufferMemory(device, buffer, memory, 0);
}

void VulkanRenderer::copy_data_to_buffer(VkBuffer dst_buffer, const void* data, VkDeviceSize size) {
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);
    void* buffer_data;
    vkdf->vkMapMemory(device, staging_buffer_memory, 0, size, 0, &buffer_data);
    memcpy(buffer_data, data, size);
    vkdf->vkUnmapMemory(device, staging_buffer_memory);

    copy_buffer(staging_buffer, dst_buffer, size);

    vkdf->vkDestroyBuffer(device, staging_buffer, nullptr);
    vkdf->vkFreeMemory(device, staging_buffer_memory, nullptr);
}

void VulkanRenderer::copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;
    vkdf->vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    end_single_time_commands(command_buffer);
}
