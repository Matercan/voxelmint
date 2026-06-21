#include <array>
#include <limits>
#include <ostream>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <fstream>
#include <chrono>
#include <string>
#include <tuple>
#include <mutex>
#include <vector>
#include <print>
#include <unordered_map>

#ifdef WINDOWS

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32

#else

#define VK_USE_PLATFORM_WAYLAND_KHR
#define GLFW_EXPOSE_NATIVE_WAYLAND

#endif // WINDOWS

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_core.h>
#include "vulkan/vulkan.hpp"
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#define GLM_FORCE_DETPH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../include/stb/stb_image.h"
#include "../include/stb/stb_image_resize2.h"

#include "vertex.hpp"

extern "C" constexpr uint32_t WIDTH                = 800;
extern "C" constexpr uint32_t HEIGHT               = 800;
extern "C" constexpr int      MAX_FRAMES_IN_FLIGHT = 2;
extern "C" constexpr uint8_t  POV                  = 60;

const std::vector<char const*> validationLayers        = {"VK_LAYER_KHRONOS_validation"};
std::vector<const char*>       requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

struct MeshUploadJob {
  std::vector<Vertex>   vertices;
  std::vector<uint32_t> indices;
  bool                  reset = false;
};

constexpr std::vector<char> readFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  std::vector<char> buffer(file.tellg());
  file.seekg(0, std::ios::beg);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

  file.close();
  return buffer;
}

constexpr bool isDeviceSuitable(vk::raii::PhysicalDevice const& physicalDevice) {
  bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;
  auto queueFamilies     = physicalDevice.getQueueFamilyProperties();
  bool supportsGraphics  = std::ranges::any_of(queueFamilies, [](auto const& qfp) {
    return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
  });

  auto availableDeviceExtensions     = physicalDevice.enumerateDeviceExtensionProperties();
  bool supportsAllRequiredExtensions = std::ranges::all_of(
    requiredDeviceExtension, [&availableDeviceExtensions](auto const& requiredDeviceExtension) {
      return std::ranges::any_of(
        availableDeviceExtensions, [requiredDeviceExtension](auto const& availableDeviceExtension) {
          return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
        });
    });

  auto features = physicalDevice.template getFeatures2<
    vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
    vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
  bool supportsRequiredFeatures =
    features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
    features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
    features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
    features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
    features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

  return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions &&
         supportsRequiredFeatures;
}

constexpr vk::SurfaceFormatKHR
chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats) {
  assert(!availableFormats.empty());
  const auto formatIt = std::ranges::find_if(availableFormats, [](const auto& format) {
    return format.format == vk::Format::eB8G8R8A8Srgb &&
           format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
  });

  return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

constexpr vk::PresentModeKHR
chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes) {
  assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) {
    return presentMode == vk::PresentModeKHR::eFifo;
  }));
  return std::ranges::any_of(
           availablePresentModes,
           [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; })
           ? vk::PresentModeKHR::eMailbox
           : vk::PresentModeKHR::eFifo;
}

constexpr uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities) {
  auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
  if ((0 < surfaceCapabilities.maxImageCount) &&
      (surfaceCapabilities.maxImageCount < minImageCount)) {
    minImageCount = surfaceCapabilities.maxImageCount;
  }
  return minImageCount;
}

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
  vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
  const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage
            << std::endl;

  return vk::False;
}

class Application {
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

  void init() {
    initWindow();
    initVulkan();
  }

  void tick() {
    glfwPollEvents();
    drawFrame();
    setDeltaTime();
  }

  void close() {
    cleanup();
  }

  void pushVertices(std::vector<Vertex> vertices, std::vector<uint32_t> indices, bool reset) {
    std::lock_guard<std::mutex> lock(uploadMutex);

    pendingUploads.push(
      {.vertices = std::move(vertices), .indices = std::move(indices), .reset = reset});
  }

  uint32_t getTextureIndex(std::string name) {
    auto faceValue = textureToIdx.find(name);
    if (faceValue == textureToIdx.end()) {
      std::string baseName =
        name.substr(0, std::distance(name.begin(), std::find(name.begin(), name.end(), '_')));
      auto baseValue = textureToIdx.find(baseName);
      return baseValue == textureToIdx.end() ? -1 : baseValue->second;
    } else {
      return faceValue->second;
    }
  }

  float deltaTimeMS() {
    return this->deltaTime;
  }

private:
  GLFWwindow*                      window         = nullptr;
  vk::raii::Instance               instance       = nullptr;
  vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
  vk::raii::PhysicalDevice         physicalDevice = nullptr;
  vk::raii::Device                 device         = nullptr;
  vk::raii::Queue                  graphicsQueue  = nullptr;
  vk::raii::SurfaceKHR             surface        = nullptr;
  vk::raii::Context                context;
  vk::PhysicalDeviceFeatures       deviceFeatures;

  vk::raii::Pipeline            graphicsPipeline    = nullptr;
  vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
  vk::raii::PipelineLayout      pipelineLayout      = nullptr;

  vk::raii::SwapchainKHR           swapChain = nullptr;
  vk::SurfaceFormatKHR             swapChainSurfaceFormat;
  vk::Extent2D                     swapChainExtent;
  std::vector<vk::Image>           swapChainImages;
  std::vector<vk::raii::ImageView> swapChainImageViews;

  std::mutex                          uploadMutex;
  std::queue<MeshUploadJob>           pendingUploads;
  std::vector<vk::raii::Buffer>       vertexBuffers;
  std::vector<vk::raii::DeviceMemory> vertexBuffersMemory;
  std::vector<void*>                  vertexBuffersMapped;
  std::vector<vk::raii::Buffer>       indexBuffers;
  std::vector<vk::raii::DeviceMemory> indexBuffersMemory;
  std::vector<void*>                  indexBuffersMapped;
  std::vector<uint32_t>               currentIndexCount;
  bool                                drawingDataAvailable = false;

  vk::raii::Image                           textureImage       = nullptr;
  vk::raii::DeviceMemory                    textureImageMemory = nullptr;
  vk::raii::ImageView                       textureImageView   = nullptr;
  vk::raii::Sampler                         textureSampler     = nullptr;
  std::unordered_map<std::string, uint32_t> textureToIdx;
  uint32_t                                  mipLevels;

  vk::raii::Image        depthImage       = nullptr;
  vk::raii::DeviceMemory depthImageMemory = nullptr;
  vk::raii::ImageView    depthImageView   = nullptr;

  std::vector<vk::raii::Buffer>       uniformBuffers;
  std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
  std::vector<void*>                  uniformBuffersMapped;

  vk::raii::DescriptorPool             descriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> descriptorSets;

  float                                deltaTime;
  uint32_t                             queueIndex  = ~0;
  uint32_t                             frameIndex  = 0;
  vk::raii::CommandPool                commandPool = nullptr;
  std::vector<vk::raii::Fence>         inFlightFences;
  std::vector<vk::raii::Semaphore>     renderFinishedSemaphores;
  std::vector<vk::raii::Semaphore>     presentCompleteSemaphores;
  std::vector<vk::raii::CommandBuffer> commandBuffers;

  void initVulkan() {
    create_instance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    pickLogicalDevice();
    createSwapChain();
    createImageViews();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createUniformBuffers();
    createCommandPool();
    createDepthResources();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createVertexBuffers();
    createIndexBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
      tick();
    }
  }

  void drawFrame() {
    auto drawFence = &inFlightFences[frameIndex];

    auto fenceResult =
      device.waitForFences(**drawFence, true, std::numeric_limits<uint64_t>::max());
    if (fenceResult != vk::Result::eSuccess) {
      throw std::runtime_error("Failed to wait for fence");
    }

    processPendingUploads();
    if (!drawingDataAvailable) {
      frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
      return;
    }

    auto [result, imageIndex] = swapChain.acquireNextImage(
      std::numeric_limits<uint64_t>::max(), presentCompleteSemaphores[frameIndex], nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR) {
      recreateSwapChain();
      return;
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
      assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
      throw std::runtime_error("failed to acquire swap chain image!");
    }

    device.resetFences(**drawFence);
    updateUniformBuffer(frameIndex);

    commandBuffers[frameIndex].reset();
    recordCommandBuffer(imageIndex);

    vk::PipelineStageFlags waitDestinationStageMask(
      vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submitInfo;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex];
    submitInfo.pWaitDstStageMask    = &waitDestinationStageMask;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &*commandBuffers[frameIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex];
    graphicsQueue.submit(submitInfo, *drawFence);

    const vk::PresentInfoKHR presentInfoKHR = {1, &*renderFinishedSemaphores[imageIndex], 1,
                                               &*swapChain, &imageIndex};
    result                                  = graphicsQueue.presentKHR(presentInfoKHR);
    frameIndex                              = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR) {
      recreateSwapChain();
    } else {
      assert(result == vk::Result::eSuccess);
    }
  }

  void updateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto  currentTime = std::chrono::high_resolution_clock::now();
    float time =
      std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view =
      lookAt(glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.proj = glm::perspective(glm::radians(static_cast<float>(POV)),
                                static_cast<float>(swapChainExtent.width) /
                                  static_cast<float>(swapChainExtent.height),
                                0.1f, 10.0f);

    ubo.proj[1][1] *= -1;
    std::memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
  }

  void processPendingUploads() {
    std::lock_guard lock(uploadMutex);

    if (pendingUploads.empty()) return;

    static std::vector<Vertex>   vertices{};
    static std::vector<uint32_t> indices{};

    while (!pendingUploads.empty()) {
      MeshUploadJob job = std::move(pendingUploads.front());
      pendingUploads.pop();

      if (job.reset) {
        vertices.clear();
        indices.clear();
      }

      vertices.insert(vertices.end(), job.vertices.begin(), job.vertices.end());
      indices.insert(indices.end(), job.indices.begin(), job.indices.end());
    }

    if (vertices.empty()) return;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      std::memcpy(vertexBuffersMapped[i], vertices.data(), vertices.size() * sizeof(vertices[0]));
      std::memcpy(indexBuffersMapped[i], indices.data(), indices.size() * sizeof(indices[0]));
      currentIndexCount[i] = static_cast<uint32_t>(indices.size());
    }

    drawingDataAvailable = true;
  }

  void setDeltaTime() {
    static auto startTime   = std::chrono::high_resolution_clock::now();
    auto        currentTime = std::chrono::high_resolution_clock::now();

    float time =
      std::chrono::duration<float, std::chrono::milliseconds::period>(currentTime - startTime)
        .count();

    startTime = currentTime;
    deltaTime = time;
  }

  void cleanupSwapChain() {
    swapChainImageViews.clear();
    depthImageView   = nullptr;
    depthImage       = nullptr;
    depthImageMemory = nullptr;
    swapChainImageViews.clear();
    swapChain = nullptr;
    swapChain = nullptr;
  }

  void cleanup() {
    device.waitIdle();

    cleanupSwapChain();
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  void create_instance() {
    constexpr vk::ApplicationInfo appInfo("Hello Triangle", VK_MAKE_VERSION(1, 0, 0), "No Engine",
                                          VK_MAKE_VERSION(1, 0, 0), vk::ApiVersion14);

    std::vector<const char*> requiredLayers;
    if (enableValidationLayers) {
      requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    auto layerProperties = context.enumerateInstanceLayerProperties();
    auto unsupportedLayerIt =
      std::ranges::find_if(requiredLayers, [&layerProperties](auto const& requiredLayer) {
        return std::ranges::none_of(layerProperties, [requiredLayer](const auto& layerProperty) {
          return strcmp(layerProperty.layerName, requiredLayer) == 0;
        });
      });
    if (unsupportedLayerIt != requiredLayers.end()) {
      throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
    }

    auto requiredExtensions = getRequiredInstanceExtensions();

    auto extensionProperties   = context.enumerateInstanceExtensionProperties();
    auto unsupportedPropertyIt = std::ranges::find_if(
      requiredExtensions, [&extensionProperties](auto const& requiredExtension) {
        return std::ranges::none_of(
          extensionProperties, [requiredExtension](auto const& extensionProperty) {
            return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
          });
      });
    if (unsupportedPropertyIt != requiredExtensions.end()) {
      throw std::runtime_error("Required extension not supported: " +
                               std::string(*unsupportedPropertyIt));
    }

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size());
    createInfo.ppEnabledLayerNames     = requiredLayers.data();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    instance = vk::raii::Instance(context, createInfo);
  }

  void setupDebugMessenger() {
    if (!enableValidationLayers) return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoExt{};
    debugUtilsMessengerCreateInfoExt.messageSeverity = severityFlags;
    debugUtilsMessengerCreateInfoExt.messageType     = messageTypeFlags;
    debugUtilsMessengerCreateInfoExt.pfnUserCallback = &debugCallback;
    debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoExt);
  }

  void pickPhysicalDevice() {
    std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
    auto const devIter = std::ranges::find_if(physicalDevices, [&](auto const& physicalDevice) {
      return isDeviceSuitable(physicalDevice);
    });
    if (devIter == physicalDevices.end()) {
      throw std::runtime_error("Failed to find a suitable GPU!");
    }
    physicalDevice = *devIter;
  }

  void pickLogicalDevice() {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
      physicalDevice.getQueueFamilyProperties();
    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++) {
      if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
          physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)) {
        queueIndex = qfpIndex;
        break;
      }
    }

    if (queueIndex == ~static_cast<uint32_t>(0)) {
      throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
    }
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo;
    float                     queuePriority = 0.5f;
    deviceQueueCreateInfo.queueFamilyIndex  = queueIndex;
    deviceQueueCreateInfo.queueCount        = 1;
    deviceQueueCreateInfo.pQueuePriorities  = &queuePriority;

    vk::PhysicalDeviceFeatures2                       pd2f;
    vk::PhysicalDeviceVulkan13Features                pdv13f;
    vk::PhysicalDeviceVulkan11Features                pdv11f;
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT pdedsfext;
    pd2f.features.samplerAnisotropy = true;
    pdv13f.dynamicRendering         = true;
    pdv13f.synchronization2         = true;
    pdv11f.shaderDrawParameters     = true;
    pdedsfext.extendedDynamicState  = true;

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceVulkan11Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      featureChain = {pd2f, pdv13f, pdv11f, pdedsfext};

    vk::DeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtension.data();
    deviceCreateInfo.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>();
    deviceCreateInfo.queueCreateInfoCount    = 1;
    deviceCreateInfo.pQueueCreateInfos       = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size());

    device        = vk::raii::Device(physicalDevice, deviceCreateInfo);
    graphicsQueue = vk::raii::Queue(device, queueIndex, 0);
  }

  void createSurface() {
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
      throw std::runtime_error("Failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
  }

  std::vector<const char*> getRequiredInstanceExtensions() {
    uint32_t glfwExtensionCount = 0;
    auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) extensions.push_back(vk::EXTDebugUtilsExtensionName);

    return extensions;
  }

  void recreateSwapChain() {
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }

    device.waitIdle();

    cleanupSwapChain();
    createSwapChain();
    createImageViews();
    createDepthResources();
  }

  void createSwapChain() {
    vk::SurfaceCapabilitiesKHR surfaceCapabilities =
      physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    swapChainExtent        = chooseSwapExtent(surfaceCapabilities);
    uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

    std::vector<vk::SurfaceFormatKHR> availableFormats =
      physicalDevice.getSurfaceFormatsKHR(*surface);
    swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

    std::vector<vk::PresentModeKHR> availablePresentModes =
      physicalDevice.getSurfacePresentModesKHR(*surface);

    vk::SwapchainCreateInfoKHR swapChainCreateInfo;
    swapChainCreateInfo.surface          = *surface;
    swapChainCreateInfo.minImageCount    = minImageCount;
    swapChainCreateInfo.imageFormat      = swapChainSurfaceFormat.format;
    swapChainCreateInfo.imageColorSpace  = swapChainSurfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent      = swapChainExtent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCreateInfo.preTransform     = surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCreateInfo.presentMode      = chooseSwapPresentMode(availablePresentModes);
    swapChainCreateInfo.clipped          = true;
    swapChainCreateInfo.oldSwapchain     = nullptr;

    swapChain       = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
    swapChainImages = swapChain.getImages();
  }

  void createImageViews() {
    assert(swapChainImageViews.empty());

    for (auto& image : swapChainImages) {
      swapChainImageViews.emplace_back(
        createImageView(image, swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor));
    }
  }

  [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const {
    vk::ShaderModuleCreateInfo createInfo;
    createInfo.codeSize = code.size() * sizeof(char);
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    vk::raii::ShaderModule shaderModule(device, createInfo);
    return shaderModule;
  }

  void createDescriptorSetLayout() {
    std::array<vk::DescriptorSetLayoutBinding, 3> bindings{
      {{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex},
       {1, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment},
       {2, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment}}};

    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings    = bindings.data();

    descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
  }

  void createGraphicsPipeline() {
    auto shaderModule = createShaderModule(readFile("src/rendering/slang.spv"));

    std::vector<vk::DynamicState>            dynamicStates = {vk::DynamicState::eViewport,
                                                              vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo       dynamicState;
    vk::PipelineShaderStageCreateInfo        vertShaderStageInfo;
    vk::PipelineShaderStageCreateInfo        fragShaderStageInfo;
    vk::PipelineVertexInputStateCreateInfo   vertexInputInfo;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    vk::PipelineLayoutCreateInfo             pipelineLayoutInfo;
    vk::PipelineMultisampleStateCreateInfo   multiSampling;
    vk::PipelineRenderingCreateInfo          pipelineRenderingCreateInfo;
    vk::PipelineColorBlendAttachmentState    colorBlendAttachment;
    vk::PipelineColorBlendStateCreateInfo    colorBlending;
    vk::PipelineRasterizationStateCreateInfo rasterizer;
    vk::PipelineViewportStateCreateInfo      viewportState;
    vk::PipelineRenderingCreateInfo          renderingInfo;
    vk::PipelineDepthStencilStateCreateInfo  depthStencil;
    vk::GraphicsPipelineCreateInfo           graphicsPipelineInfo;

    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    vertShaderStageInfo.stage  = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = shaderModule;
    vertShaderStageInfo.pName  = "vertMain";
    fragShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = shaderModule;
    fragShaderStageInfo.pName  = "fragMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    inputAssembly.topology             = vk::PrimitiveTopology::eTriangleList;
    rasterizer.depthClampEnable        = false;
    rasterizer.rasterizerDiscardEnable = false;
    rasterizer.polygonMode             = vk::PolygonMode::eFill;
    rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace               = vk::FrontFace::eClockwise;
    rasterizer.depthClampEnable        = false;
    rasterizer.lineWidth               = 1.0f;

    multiSampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multiSampling.sampleShadingEnable  = false;

    colorBlendAttachment.blendEnable = false;
    colorBlendAttachment.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    colorBlending.logicOpEnable   = false;
    colorBlending.logicOp         = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pSetLayouts            = &*descriptorSetLayout;

    pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

    pipelineRenderingCreateInfo.colorAttachmentCount    = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainSurfaceFormat.format;

    auto bindingDescrption                        = getVertexBindingDescription();
    auto attributeDescriptions                    = getVertexAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions    = &bindingDescrption;
    vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    depthStencil.depthTestEnable       = true;
    depthStencil.depthWriteEnable      = true;
    depthStencil.depthCompareOp        = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = false;
    depthStencil.stencilTestEnable     = false;

    graphicsPipelineInfo.stageCount          = 2;
    graphicsPipelineInfo.pStages             = shaderStages;
    graphicsPipelineInfo.pVertexInputState   = &vertexInputInfo;
    graphicsPipelineInfo.pInputAssemblyState = &inputAssembly;
    graphicsPipelineInfo.pViewportState      = &viewportState;
    graphicsPipelineInfo.pRasterizationState = &rasterizer;
    graphicsPipelineInfo.pMultisampleState   = &multiSampling;
    graphicsPipelineInfo.pColorBlendState    = &colorBlending;
    graphicsPipelineInfo.pDynamicState       = &dynamicState;
    graphicsPipelineInfo.layout              = pipelineLayout;
    graphicsPipelineInfo.renderPass          = nullptr;
    graphicsPipelineInfo.pDepthStencilState  = &depthStencil;

    pipelineRenderingCreateInfo.colorAttachmentCount    = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainSurfaceFormat.format;
    pipelineRenderingCreateInfo.depthAttachmentFormat   = findDepthFormat();

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo>
      pipelineCreateInfoChain = {graphicsPipelineInfo, pipelineRenderingCreateInfo};

    graphicsPipeline = vk::raii::Pipeline(
      device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
  }

  void createCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                       queueIndex};
    commandPool = vk::raii::CommandPool(device, poolInfo);
  }

  void createDepthResources() {
    vk::Format depthFormat = findDepthFormat();
    std::tie(depthImage, depthImageMemory) =
      createImage(swapChainExtent.width, swapChainExtent.height, depthFormat,
                  vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, 1, 1);

    depthImageView = createImageView(*depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
  }

  void createTextureImage() {
    TexManager* textures     = getTexManager();
    auto        texturesSize = getTexturesSize(textures);
    auto        textureCount = getTextureCount(textures);
    auto        faceCount    = getFaceCount(textures);
    auto        textureSize  = getTextureSize(textures);

    auto [stagingBuffer, stagingBufferMemory] = createBuffer(
      texturesSize, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void* data = stagingBufferMemory.mapMemory(0, texturesSize);

    std::print("Image width: {}, Image height: {}\n", textureSize[0], textureSize[1]);

    uint64_t offset = 0;
    mipLevels =
      static_cast<uint32_t>(std::floor(std::log2(std::max(textureSize[0], textureSize[1])))) + 1;

    vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommandBuffer();

    std::tie(textureImage, textureImageMemory) = createImage(
      textureSize[0], textureSize[1], vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eSampled,
      vk::MemoryPropertyFlagBits::eDeviceLocal, faceCount + 1, mipLevels);

    transitionImageLayout(commandBuffer, textureImage, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal, faceCount, mipLevels);

    size_t totalCount = 0;
    for (uint32_t i = 0; i < textureCount; i++) {

      uint8_t      size;
      FaceTexture* faceTextures = getNextTextures(textures, &size);

      for (size_t j = 0; j < size; j++) {
        if (!faceTextures[j].data) {
          totalCount++;
          continue;
        }

        size_t imageSize = faceTextures[j].width * faceTextures[j].height * 4;
        std::memcpy((char*)data + offset, faceTextures[j].data, imageSize);

        // std::println("Current offset: {}, Current index: {}", offset, totalCount);
        // std::println("  Width: {}, Height: {}", faceTextures[j].width, faceTextures[j].height);

        vk::BufferImageCopy region{offset,
                                   0,
                                   0,
                                   {vk::ImageAspectFlagBits::eColor, 0, (uint32_t)totalCount,
                                    1},       // baseArrayLayer=totalCount, layerCount=1
                                   {0, 0, 0}, // imageOffset all zeros
                                   {textureSize[0], textureSize[1], 1}};

        commandBuffer.copyBufferToImage(stagingBuffer, textureImage,
                                        vk::ImageLayout::eTransferDstOptimal, region);

        std::string label = (std::strcmp(faceTextures[j].label, "front") == 0)
                              ? std::string{}
                              : std::string(faceTextures[j].label);

        auto        name = std::string(faceTextures[j].baseTexture);
        std::string baseName =
          name.substr(0, std::distance(name.begin(), std::find(name.begin(), name.end(), '.')));
        std::string fullName = baseName + (label.empty() ? "" : "_") + std::string(label) + ".gtex";
        textureToIdx.insert({fullName, totalCount});

        stbi_image_free(faceTextures[j].data);
        std::free(faceTextures[j].label);

        offset += imageSize;
        totalCount++;
      }
    }

    stagingBufferMemory.unmapMemory();
    freeTexManager(textures);

    generateMipmaps(commandBuffer, textureImage, vk::Format::eR8G8B8A8Srgb, textureSize[0], textureSize[1],
                    textureCount, mipLevels);
    transitionImageLayout(commandBuffer, textureImage, vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal, faceCount, mipLevels);
    endSingleTimeCommandBuffer(std::move(commandBuffer));

    std::free(textureSize);
  }

  void createTextureImageView() {
    TexManager*             textures = getTexManager();
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image            = textureImage;
    viewInfo.viewType         = vk::ImageViewType::e2DArray;
    viewInfo.format           = vk::Format::eR8G8B8A8Srgb;
    viewInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0,
                                 (uint32_t)getFaceCount(textures)};
    textureImageView          = vk::raii::ImageView(device, viewInfo);
    freeTexManager(textures);
  }

  void createTextureSampler() {
    vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
    vk::SamplerCreateInfo        samplerInfo;
    samplerInfo.magFilter               = {};
    samplerInfo.minFilter               = {};
    samplerInfo.mipmapMode              = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU            = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV            = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW            = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable        = vk::True;
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.compareEnable           = vk::False;
    samplerInfo.compareOp               = vk::CompareOp::eAlways;
    samplerInfo.unnormalizedCoordinates = vk::False;
    samplerInfo.mipmapMode              = vk::SamplerMipmapMode::eLinear;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = vk::LodClampNone;

    textureSampler = vk::raii::Sampler(device, samplerInfo);
  }

  void createVertexBuffers() {
    constexpr vk::DeviceSize bufferSize = sizeof(Vertex) * 1024;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      auto [stagingBuffer, stagingBufferMemory] = createBuffer(
        bufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

      vertexBuffers.emplace_back(std::move(stagingBuffer));
      vertexBuffersMemory.emplace_back(std::move(stagingBufferMemory));
      vertexBuffersMapped.emplace_back(vertexBuffersMemory.back().mapMemory(0, bufferSize));
    }
  }

  void createIndexBuffers() {
    constexpr vk::DeviceSize bufferSize = sizeof(uint32_t) * 1024;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      auto [stagingBuffer, stagingBufferMemory] = createBuffer(
        bufferSize, vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

      indexBuffers.emplace_back(std::move(stagingBuffer));
      indexBuffersMemory.emplace_back(std::move(stagingBufferMemory));
      indexBuffersMapped.emplace_back(indexBuffersMemory.back().mapMemory(0, bufferSize));
      currentIndexCount.push_back(0);
    }
  }

  void createUniformBuffers() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
      auto [buffer, bufferMem]  = createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                                               vk::MemoryPropertyFlagBits::eHostVisible |
                                                 vk::MemoryPropertyFlagBits::eHostCoherent);
      uniformBuffers.emplace_back(std::move(buffer));
      uniformBuffersMemory.emplace_back(std::move(bufferMem));
      uniformBuffersMapped.emplace_back(uniformBuffersMemory.back().mapMemory(0, bufferSize));
    }
  }

  void createDescriptorPool() {
    std::array<vk::DescriptorPoolSize, 3> poolSize{
      {{vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT},
       {vk::DescriptorType::eSampledImage, MAX_FRAMES_IN_FLIGHT},
       {vk::DescriptorType::eSampler, MAX_FRAMES_IN_FLIGHT}}};
    vk::DescriptorPoolCreateInfo poolInfo{vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                          MAX_FRAMES_IN_FLIGHT,
                                          static_cast<uint32_t>(poolSize.size()), poolSize.data()};

    descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
  }

  void createDescriptorSets() {
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{descriptorPool, static_cast<uint32_t>(layouts.size()),
                                            layouts.data()};

    descriptorSets = device.allocateDescriptorSets(allocInfo);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DescriptorBufferInfo bufferInfo(uniformBuffers[i], 0, sizeof(UniformBufferObject));

      vk::DescriptorImageInfo imageInfo({}, textureImageView,
                                        vk::ImageLayout::eShaderReadOnlyOptimal);

      vk::DescriptorImageInfo samplerInfo(textureSampler, {}, {});

      vk::WriteDescriptorSet bWrite;
      bWrite.dstSet          = descriptorSets[i];
      bWrite.dstBinding      = 0;
      bWrite.dstArrayElement = 0;
      bWrite.descriptorCount = 1;
      bWrite.descriptorType  = vk::DescriptorType::eUniformBuffer;
      bWrite.pBufferInfo     = &bufferInfo;

      vk::WriteDescriptorSet iWrite;
      iWrite.dstSet          = descriptorSets[i];
      iWrite.dstBinding      = 1;
      iWrite.dstArrayElement = 0;
      iWrite.descriptorCount = 1;
      iWrite.descriptorType  = vk::DescriptorType::eSampledImage;
      iWrite.pImageInfo      = &imageInfo;

      vk::WriteDescriptorSet sWrite;
      sWrite.dstSet          = descriptorSets[i];
      sWrite.dstBinding      = 2;
      sWrite.dstArrayElement = 0;
      sWrite.descriptorCount = 1;
      sWrite.descriptorType  = vk::DescriptorType::eSampler;
      sWrite.pImageInfo      = &samplerInfo;

      device.updateDescriptorSets(std::array<vk::WriteDescriptorSet, 3>{bWrite, iWrite, sWrite},
                                  {});
    }
  }

  void createCommandBuffers() {
    vk::CommandBufferAllocateInfo allocInfo{commandPool, vk::CommandBufferLevel::ePrimary, 2};
    commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
  }

  void createSyncObjects() {
    assert(presentCompleteSemaphores.empty() && inFlightFences.empty() &&
           renderFinishedSemaphores.empty());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
      renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      presentCompleteSemaphores.emplace_back(
        vk::raii::Semaphore(device, vk::SemaphoreCreateInfo()));
      inFlightFences.emplace_back(vk::raii::Fence(device, {vk::FenceCreateFlagBits::eSignaled}));
    }
  }

  void recordCommandBuffer(uint32_t imageIndex) {
    auto commandBuffer = &commandBuffers[frameIndex];

    commandBuffer->begin({});
    transitionImageLayout(
      swapChainImages[imageIndex], vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal, {}, vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);

    transitionImageLayout(*depthImage, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthAttachmentOptimal,
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                            vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                            vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::ImageAspectFlagBits::eDepth);

    vk::ClearValue              clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::ClearValue              clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
    vk::RenderingAttachmentInfo attatchmentInfo;
    vk::RenderingAttachmentInfo depthAttachmentInfo;
    vk::RenderingInfo           renderingInfo;

    attatchmentInfo.imageView   = swapChainImageViews[imageIndex];
    attatchmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attatchmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    attatchmentInfo.storeOp     = vk::AttachmentStoreOp::eStore;
    attatchmentInfo.clearValue  = clearColor;

    depthAttachmentInfo.imageView   = depthImageView;
    depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
    depthAttachmentInfo.storeOp     = vk::AttachmentStoreOp::eDontCare;
    depthAttachmentInfo.clearValue  = clearDepth;

    renderingInfo.renderArea           = {.offset = {0, 0}, .extent = swapChainExtent};
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &attatchmentInfo;
    renderingInfo.pDepthAttachment     = &depthAttachmentInfo;

    commandBuffer->beginRendering(renderingInfo);
    commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
    commandBuffer->bindVertexBuffers(0, *vertexBuffers[frameIndex], {0});
    commandBuffer->bindIndexBuffer(*indexBuffers[frameIndex], 0, vk::IndexType::eUint32);

    commandBuffer->setViewport(0,
                               vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width),
                                            static_cast<float>(swapChainExtent.height)));
    commandBuffer->setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
    commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
                                      *descriptorSets[frameIndex], nullptr);
    commandBuffer->drawIndexed(static_cast<uint32_t>(currentIndexCount[frameIndex]), 1, 0, 0, 0);
    commandBuffer->endRendering();

    transitionImageLayout(
      swapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eColorAttachmentWrite, {},
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe,
      vk::ImageAspectFlagBits::eColor);

    commandBuffer->end();
  }

  void transitionImageLayout(vk::Image image, vk::ImageLayout old_layout,
                             vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask,
                             vk::AccessFlags2        dst_access_mask,
                             vk::PipelineStageFlags2 src_stage_mask,
                             vk::PipelineStageFlags2 dst_stage_mask,
                             vk::ImageAspectFlags    image_aspect_flags) {
    vk::ImageMemoryBarrier2 barrier;
    vk::DependencyInfo      dependency_info;

    barrier.subresourceRange                = {image_aspect_flags, 0, 1, 0, 1};
    barrier.srcAccessMask                   = src_access_mask;
    barrier.srcStageMask                    = src_stage_mask;
    barrier.dstStageMask                    = dst_stage_mask;
    barrier.dstAccessMask                   = dst_access_mask;
    barrier.oldLayout                       = old_layout;
    barrier.newLayout                       = new_layout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    dependency_info.dependencyFlags         = {};
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers    = &barrier;

    commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
  }

  void transitionImageLayout(vk::raii::CommandBuffer& commandBuffer, const vk::raii::Image& image,
                             vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                             uint32_t layerCount = 1, uint32_t mipLevels = 1) {
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image               = image;
    barrier.subresourceRange    = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, layerCount};

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eTransferDstOptimal) {
      barrier.srcAccessMask = {};
      barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

      sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
      destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
               newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
      barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

      sourceStage      = vk::PipelineStageFlagBits::eTransfer;
      destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
      throw std::invalid_argument("unsupported layout transition!");
    }
    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
  }

  constexpr bool hasStencilComponent(vk::Format format) {
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
  }

  vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
                                 vk::FormatFeatureFlags features) {
    auto formatIt = std::ranges::find_if(candidates, [&](auto const format) {
      vk::FormatProperties props = physicalDevice.getFormatProperties(format);
      return (((tiling == vk::ImageTiling::eLinear) &&
               ((props.linearTilingFeatures & features) == features)) ||
              ((tiling == vk::ImageTiling::eOptimal) &&
               ((props.optimalTilingFeatures & features) == features)));
    });
    if (formatIt == candidates.end()) {
      throw std::runtime_error("failed to find supported format!");
    }
    return *formatIt;
  }

  vk::Format findDepthFormat() {
    return findSupportedFormat(
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
  }

  constexpr std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>
  createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags properties) {
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    vk::raii::Buffer       buffer          = vk::raii::Buffer(device, bufferInfo);
    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo memoryAllocateInfo{
      memRequirements.size, findMemoryType(memRequirements.memoryTypeBits, properties)};

    vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
    buffer.bindMemory(*bufferMemory, 0);
    return {std::move(buffer), std::move(bufferMemory)};
  }

  constexpr vk::raii::ImageView createImageView(vk::Image const& image, vk::Format format,
                                                vk::ImageAspectFlags aspectFlags,
                                                uint32_t textureCount = 1, uint32_t mipLevels = 1) {
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image            = image;
    viewInfo.viewType         = vk::ImageViewType::e2D;
    viewInfo.format           = format;
    viewInfo.subresourceRange = {aspectFlags, 0, mipLevels, 0, textureCount};
    return vk::raii::ImageView(device, viewInfo);
  }

  constexpr std::pair<vk::raii::Image, vk::raii::DeviceMemory>
  createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling,
              vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, uint32_t textureCount,
              uint32_t mipLevels) {
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType   = vk::ImageType::e2D;
    imageInfo.format      = format;
    imageInfo.extent      = (vk::Extent3D){width, height, 1};
    imageInfo.mipLevels   = 1;
    imageInfo.arrayLayers = textureCount;
    imageInfo.samples     = vk::SampleCountFlagBits::e1;
    imageInfo.mipLevels   = mipLevels;
    imageInfo.tiling      = tiling;
    imageInfo.usage       = usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    vk::raii::Image image = vk::raii::Image(device, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{memRequirements.size,
                                     findMemoryType(memRequirements.memoryTypeBits, properties)};
    vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(device, allocInfo);
    image.bindMemory(imageMemory, 0);

    return {std::move(image), std::move(imageMemory)};
  };

  constexpr vk::raii::CommandBuffer beginSingleTimeCommandBuffer() {
    vk::CommandBufferAllocateInfo allocInfo{commandPool, vk::CommandBufferLevel::ePrimary, 1};
    vk::raii::CommandBuffer       commandBuffer =
      std::move(device.allocateCommandBuffers(allocInfo).front());

    commandBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    return commandBuffer;
  }

  void endSingleTimeCommandBuffer(vk::raii::CommandBuffer&& commandBuffer) {
    commandBuffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &*commandBuffer;
    graphicsQueue.submit(submitInfo, nullptr);
    graphicsQueue.waitIdle();
  }

  void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size) {
    vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommandBuffer();
    commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));
    endSingleTimeCommandBuffer(std::move(commandCopyBuffer));
  }

  void generateMipmaps(vk::raii::CommandBuffer& commandBuffer, vk::raii::Image& image,
                       vk::Format imageFormat, int32_t texWidth, int32_t texHeight,
                       uint32_t texCount, uint32_t mipLevels) {
    vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(imageFormat);
    if (!(formatProperties.optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
      throw std::runtime_error("texture image format does not support linear blitting!");
    }

    vk::ImageMemoryBarrier barrier(
      vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
      vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
      vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, image);
    barrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = texCount;
    barrier.subresourceRange.levelCount     = 1;

    int32_t mipWidth  = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
      barrier.newLayout                     = vk::ImageLayout::eTransferSrcOptimal;
      barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
      barrier.dstAccessMask                 = vk::AccessFlagBits::eTransferRead;

      commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                    vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

      vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
      offsets[0]    = vk::Offset3D(0, 0, 0);
      offsets[1]    = vk::Offset3D(mipWidth, mipHeight, 1);
      dstOffsets[0] = vk::Offset3D(0, 0, 0);
      dstOffsets[1] =
        vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1);
      vk::ImageBlit blit = {{}, offsets, {}, dstOffsets};
      blit.srcSubresource =
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, texCount);
      blit.dstSubresource =
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, texCount);
      commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image,
                              vk::ImageLayout::eTransferDstOptimal, {blit}, vk::Filter::eLinear);

      barrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
      barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
      barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

      commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                    vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {},
                                    barrier);

      if (mipWidth > 1) mipWidth /= 2;
      if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout                     = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask                 = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                  vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
  }

  constexpr uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
      if ((typeFilter & (1 << i)) &&
          (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
        return i;
      }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
  }

  constexpr vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                 capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                 capabilities.maxImageExtent.height)};
  }

  void initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
  }
};

extern "C" {
Application* getApplication() {
  return new Application;
}

int run() {
  auto app = getApplication();

  try {
    app->run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

void initApplication(Application* app) {
  app->init();
}

void tickApplication(Application* app) {
  app->tick();
}

void closeApplication(Application* app) {
  app->close();
  delete app;
}

void setVertices(Application* app, Vertex* vertices, size_t vert_len, uint32_t* indices,
                 size_t ind_len) {
  std::vector<Vertex>   verts(vertices, vertices + vert_len);
  std::vector<uint32_t> inds(indices, indices + ind_len);
  app->pushVertices(std::move(verts), std::move(inds), true);
}

void pushVertices(Application* app, Vertex* vertices, size_t vert_len, uint32_t* indices,
                  size_t ind_len) {
  std::vector<Vertex>   verts(vertices, vertices + vert_len);
  std::vector<uint32_t> inds(indices, indices + ind_len);
  app->pushVertices(std::move(verts), std::move(inds), false);
}

uint32_t getTextureIndex(Application* app, char* name) {
  std::string textureName{name};
  return app->getTextureIndex(textureName);
}

float getDeltaTime(Application* app) {
  return app->deltaTimeMS();
}
}
