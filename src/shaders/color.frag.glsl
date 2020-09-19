#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 o_color;

layout(location = 0) in vec3 v_color;
layout(location = 1) in vec2 v_tex_coord;

layout(binding = 1) uniform sampler2D tex_sampler;

void main() {
    o_color = texture(tex_sampler, v_tex_coord);
}
