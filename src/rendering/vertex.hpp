#pragma once

#include <array>
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
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
};

struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

vk::VertexInputBindingDescription                  getVertexBindingDescription();
std::array<vk::VertexInputAttributeDescription, 3> getVertexAttributeDescriptions();

size_t vertexSize();
size_t vertexCount();
