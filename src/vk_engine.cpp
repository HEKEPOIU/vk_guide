//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"
#include <chrono>
#include <thread>

namespace {
VulkanEngine *loadedEngine = nullptr;
auto constexpr bUseValidationLayers = true;
}; // namespace

VulkanEngine &VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init() {
  // only one engine initialization is allowed with the application.
  assert(loadedEngine == nullptr);
  loadedEngine = this;

  // We initialize SDL and create a window with it.
  SDL_Init(SDL_INIT_VIDEO);

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

  _window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, _windowExtent.width,
                             _windowExtent.height, window_flags);
  if (!_window) {
    fmt::println(stderr, "Can't Create window, SDLError: {}", SDL_GetError());
    std::abort();
  }

  init_vulkan();
  init_swapchain();
  init_commands();
  init_sync_structures();

  // everything went fine
  _isInitialized = true;
}

void VulkanEngine::init_vulkan() {
  vkb::InstanceBuilder builder;

  auto const inst_ret = builder.set_app_name("Example Vulkan Application")
                            .request_validation_layers(bUseValidationLayers)
                            .use_default_debug_messenger()
                            .require_api_version(1, 3, 0)
                            .build();

  RESULT_CHECK(inst_ret, "Can't Init Vulkan Instance, Error: {}");
  _instance = inst_ret->instance;
  _debug_messager = inst_ret->debug_messenger;

  if (SDL_Vulkan_CreateSurface(_window, _instance, &_surface) != SDL_TRUE) {
    fmt::println(stderr, "Can't create Vulkan Surface, error: {}",
                 SDL_GetError());
    std::abort();
  }

  VkPhysicalDeviceVulkan13Features features_13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .synchronization2 = true,
      .dynamicRendering = true,
  };
  VkPhysicalDeviceVulkan12Features features_12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .descriptorIndexing = true,
      .bufferDeviceAddress = true,
  };

  vkb::PhysicalDeviceSelector selector{*inst_ret, _surface};

  vkb::Result<vkb::PhysicalDevice> physicalDevice =
      selector.set_minimum_version(1, 3)
          .set_required_features_13(features_13)
          .set_required_features_12(features_12)
          .select();

  RESULT_CHECK(physicalDevice, "Can't select suitable PhysicalDevice, errormessage: {}");

  vkb::DeviceBuilder deviceBuilder{*physicalDevice};
  vkb::Result<vkb::Device> vkbDevice = deviceBuilder.build();

  RESULT_CHECK(vkbDevice, "Failed to create physical device, errormessage: {}");

  _device = vkbDevice->device;
  _chosenGPU = vkbDevice->physical_device;

  auto graphicsQueueResult = vkbDevice->get_queue(vkb::QueueType::graphics);
  RESULT_CHECK(graphicsQueueResult, "Fail to get graphics queue, errormessage: {}");

  _graphicsQueue = *graphicsQueueResult;

  auto graphicsQueueFamilyResult = vkbDevice->get_queue_index(vkb::QueueType::graphics);
  RESULT_CHECK(graphicsQueueFamilyResult, "Failed to get Graphics Queue Family, errormessage: {}");
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
  vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
  _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
  vkb::Result<vkb::Swapchain> vkbSwapchain =
      swapchainBuilder
          .set_desired_format(VkSurfaceFormatKHR{
              .format = _swapchainImageFormat,
              .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(width, height)
          .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
          .build();
  if (!vkbSwapchain) {
    fmt::println(stderr, "Failed to create swapchain, errormessage: {}",
                 vkbSwapchain.error().message());
    std::abort();
  }

  _swapchainExtent = vkbSwapchain->extent;
  _swapchain = vkbSwapchain->swapchain;
  auto images = vkbSwapchain->get_images();
  if (!images) {
    fmt::println(stderr, "Failed to get image, errormessage: {}",
                 images.error().message());
    std::abort();
  }
  _swapchainImage = *images;

  auto image_views = vkbSwapchain->get_image_views();

  if (!image_views) {
    fmt::println(stderr, "Failed to get image view, errormessage: {}",
                 image_views.error().message());
    std::abort();
  }

  _swapchainImageViews = *image_views;
}

void VulkanEngine::init_swapchain() {
  create_swapchain(_windowExtent.width, _windowExtent.height);
}

void VulkanEngine::destory_swapchain() {
  vkDestroySwapchainKHR(_device, _swapchain, nullptr);

  for (auto &imgView : _swapchainImageViews) {
    vkDestroyImageView(_device, imgView, nullptr);
  }
}

void VulkanEngine::init_commands() {}

void VulkanEngine::init_sync_structures() {}

void VulkanEngine::cleanup() {
  if (_isInitialized) {
    destory_swapchain();
    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyDevice(_device, nullptr);

    vkb::destroy_debug_utils_messenger(_instance, _debug_messager);
    vkDestroyInstance(_instance, nullptr);
    SDL_DestroyWindow(_window);
  }

  // clear engine pointer
  loadedEngine = nullptr;
}

void VulkanEngine::draw() {
  // nothing yet
}

void VulkanEngine::run() {
  SDL_Event e;
  bool bQuit = false;

  // main loop
  while (!bQuit) {
    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
      // close the window when user alt-f4s or clicks the X button
      if (e.type == SDL_QUIT)
        bQuit = true;

      if (e.type == SDL_WINDOWEVENT) {
        if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
          stop_rendering = true;
        }
        if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
          stop_rendering = false;
        }
      }
    }

    // do not draw if we are minimized
    if (stop_rendering) {
      // throttle the speed to avoid the endless spinning
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    draw();
  }
}
