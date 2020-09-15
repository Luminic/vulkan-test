#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 i_position;
layout(location = 1) in vec3 i_color;

layout(location = 0) out vec3 v_color;

layout(binding=0) uniform MVP_UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} mvp_ubo;

void main() {
    vec4 position = mvp_ubo.projection*mvp_ubo.view*mvp_ubo.model * vec4(i_position, 0.0, 1.0);
    gl_Position = vec4(position.x, -position.y, position.zw);
    v_color = i_color;
}