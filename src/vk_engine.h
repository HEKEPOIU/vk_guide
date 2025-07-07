// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_descriptors.h"
#include <vk_types.h>

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;
  void push_function(std::function<void()> &&function) {
    deletors.push_back(std::move(function));
  }
  void flush() {
    for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
      (*it)();
    }
    deletors.clear();
  }
};

struct FrameData {
  VkCommandPool _commandPool;
  VkCommandBuffer _mainCommandBuffer;
  VkSemaphore _swapchainSemaphore, _renderSemaphore;
  VkFence _renderFence;
  DeletionQueue _deletionQueue;
};

#define SecondsInNano(x) (x * 1000000000LL)

class VulkanEngine {
public:
  std::vector<FrameData> _frames;
  inline FrameData &get_current_frame() {
    return _frames[_frameNumber % _frames.size()];
  }
  inline uint32_t get_frame_overlap() { return _frames.size(); }
  VkQueue _graphicsQueue;
  uint32_t _graphicsQueueFamily;

  bool _isInitialized{false};
  int _frameNumber{0};
  bool stop_rendering{false};
  VkExtent2D _windowExtent{800, 600};
  VkInstance _instance;
  VkDebugUtilsMessengerEXT _debug_messager;
  VkPhysicalDevice _chosenGPU;
  VkDevice _device;
  VkSurfaceKHR _surface;
  VkSwapchainKHR _swapchain;
  VkFormat _swapchainImageFormat;

  std::vector<VkImage> _swapchainImage;
  std::vector<VkImageView> _swapchainImageViews;
  VkExtent2D _swapchainExtent;
  DeletionQueue _mainDeletionQueue;
  VmaAllocator _allocator;
  AllocatedImage _drawImage;
  VkExtent2D _drawExtent;
  DescriptorAllocator _globalDescriptorAllocator;

  VkFence _immFence;
  VkCommandBuffer _immCommandBuffer;
  VkCommandPool _immCommandPool;

  VkDescriptorSet _drawImageDescriptors;
  VkDescriptorSetLayout _drawImageDescriptorLayout;

  VkPipeline _gradientPipeline;
  VkPipelineLayout _gradientPipelineLayout;

  struct SDL_Window *_window{nullptr};

  static VulkanEngine &Get();

  void immediate_submit(std::function<void (VkCommandBuffer cmd)> && function);
  // initializes everything in the engine
  void init();

  // shuts down the engine
  void cleanup();

  // draw loop
  void draw();

  // run main loop
  void run();

private:
  void init_imgui();
  void draw_imgui(VkCommandBuffer cmd, VkImageView image);
  void init_pipelines();
  void init_background_pipeline();
  void init_vulkan();
  void init_swapchain();
  void init_commands();
  void init_sync_structures();
  void init_descriptors();
  void draw_background(VkCommandBuffer cmd);
  void create_swapchain(uint32_t wigth, uint32_t height);
  void destory_swapchain();
};
