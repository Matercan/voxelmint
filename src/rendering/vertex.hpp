#pragma once

#include <array>
#include <functional>
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  constexpr static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
    return {{{0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)},
             {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)},
             {2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)}}};
  }

  bool operator==(const Vertex& other) const {
    return pos == other.pos && color == other.color && texCoord == other.texCoord;
  }
};

namespace std {
template <> struct hash<Vertex> {
  size_t operator()(Vertex const& vertex) const {
    return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
           (hash<glm::vec2>()(vertex.texCoord) << 1);
  }
};
} // namespace std

struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

vk::VertexInputBindingDescription                  getVertexBindingDescription();
std::array<vk::VertexInputAttributeDescription, 3> getVertexAttributeDescriptions();

size_t vertexSize();
size_t vertexCount();
