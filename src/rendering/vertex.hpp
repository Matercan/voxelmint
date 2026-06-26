#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;
  uint32_t  texIndex;

  constexpr static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
    return {{
      {0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)},
      {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)},
      {2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)},
      {3, 0, vk::Format::eR32Uint, offsetof(Vertex, texIndex)},
    }};
  }
};

struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

vk::VertexInputBindingDescription                  getVertexBindingDescription();
std::array<vk::VertexInputAttributeDescription, 4> getVertexAttributeDescriptions();

size_t vertexSize();
size_t vertexCount();

extern "C" {
struct TexManager;
struct FaceTexture {
  unsigned char* data;
  char*          label;
  char*          baseTexture;
  uint32_t       texIdx;
  uint16_t       width, height;
};

TexManager*  getTexManager();
void         freeTexManager(TexManager*);
int64_t      getTextureCount(TexManager*);
size_t       getFaceCount(TexManager*);
size_t       getTexturesSize(TexManager*);
uint32_t*     getTextureSize(TexManager*);
FaceTexture* getNextTextures(TexManager*, uint8_t*);
}
