#ifndef VERTEX_HPP
#define VERTEX_HPP

#include <array>
#include <QVulkanFunctions>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 tex_coord;

    static VkVertexInputBindingDescription get_binding_description();
    static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions();
};

typedef uint32_t Index;

inline VkIndexType get_index_type() {
    return VK_INDEX_TYPE_UINT32;
}

#endif