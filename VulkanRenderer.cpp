#include "VulkanRenderer.hpp"

#include <QVulkanDeviceFunctions>
#include <QFile>

#include "Shader.hpp"

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

    create_graphics_pipeline();
}

void VulkanRenderer::init_swap_chain_resources() {
    qDebug() << "init_swap_chain_resources";
}

void VulkanRenderer::release_swap_chain_resources() {
    qDebug() << "release_swap_chain_resources";
}

void VulkanRenderer::release_resources() {
    qDebug() << "release_resources";

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
    vkdf->vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkdf->vkCmdEndRenderPass(command_buffer);

    vulkan_window->frame_ready();
    vulkan_window->requestUpdate();
}

void VulkanRenderer::create_graphics_pipeline() {
    VkResult res;

    ShaderModule vertex_shader_module(device, vkdf, "shaders/vert.spv");
    ShaderModule fragment_shader_module(device, vkdf, "shaders/frag.spv");

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vertex_shader_module.get_create_info(VK_SHADER_STAGE_VERTEX_BIT),
        fragment_shader_module.get_create_info(VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    // VkExtent2D extent = vulkan_window->get_image_extent();

    // VkViewport viewport{};
    // viewport.x = 0.0f;
    // viewport.y = 0.0f;
    // viewport.width = extent.width;
    // viewport.height = extent.height;
    // viewport.minDepth = 0.0f;
    // viewport.maxDepth = 1.0f;

    // qDebug() << "extent" << extent.width << extent.height;

    // VkRect2D scissor{};
    // scissor.offset = {0, 0};
    // scissor.extent = extent;

    // Viewport & Scissor are dynamically set at runtime
    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    // viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    // viewport_info.pScissors = &scissor;

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
    pipeline_layout_info.setLayoutCount = 0;
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
