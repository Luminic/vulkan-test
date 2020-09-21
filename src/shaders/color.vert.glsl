#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec2 a_tex_coord;

layout(location = 0) out vec3 v_color;
layout(location = 1) out vec2 v_tex_coord;

layout(std140, set=0, binding=0) uniform MVP_UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} mvp_ubo;

void main() {
    vec4 position = mvp_ubo.projection*mvp_ubo.view*mvp_ubo.model * vec4(a_position, 1.0);
    gl_Position = vec4(position.x, -position.y, position.zw);
    v_color = a_color;
    v_tex_coord = vec2(a_tex_coord.x, 1.0f-a_tex_coord.y);
}