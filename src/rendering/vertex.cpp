#include <array>
#include <cstddef>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>
#include "vulkan/vulkan.hpp"

#include "vertex.hpp"

vk::VertexInputBindingDescription getVertexBindingDescription() {
  return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
}
std::array<vk::VertexInputAttributeDescription, 3> getVertexAttributeDescriptions() {
  return Vertex::getAttributeDescriptions();
}

size_t vertexSize() {
  return sizeof(Vertex);
}
