#ifndef VERTEX_HPP
#define VERTEX_HPP

#include <array>
#include <QVulkanFunctions>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec2 position;
    glm::vec3 color;

    static VkVertexInputBindingDescription get_binding_description();
    static std::array<VkVertexInputAttributeDescription, 2> get_attribute_descriptions();
};

typedef uint32_t Index;

inline VkIndexType get_index_type() {
    return VK_INDEX_TYPE_UINT32;
}

#endif