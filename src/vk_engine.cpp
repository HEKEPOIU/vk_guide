//> includes
#include "vk_engine.h"
#include "vk_images.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"
#include "vk_pipelines.h"
#include <chrono>
#include <thread>
#define VMA_IMPLEMENTATION
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <vk_mem_alloc.h>

#include "constant.h"
namespace {
VulkanEngine *loadedEngine = nullptr;
auto constexpr bUseValidationLayers = true;
uint32_t get_minimum_surface_image_count(VkPhysicalDevice device,
                                         VkSurfaceKHR surface) {
  VkSurfaceCapabilitiesKHR cap;
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &cap));
  return cap.minImageCount;
}

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
  init_descriptors();
  init_pipelines();
  init_imgui();

  // everything went fine
  _isInitialized = true;
}

void VulkanEngine::init_imgui() {
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 1000,
      .poolSizeCount = std::size(pool_sizes),
      .pPoolSizes = pool_sizes};

  VkDescriptorPool imguiPool;
  VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));
  ImGui::CreateContext();
  ImGui_ImplSDL2_InitForVulkan(_window);
  ImGui_ImplVulkan_InitInfo init_info{
      .Instance = _instance,
      .PhysicalDevice = _chosenGPU,
      .Device = _device,
      .Queue = _graphicsQueue,
      .DescriptorPool = imguiPool,
      .MinImageCount = get_frame_overlap(),
      .ImageCount = get_frame_overlap(),
      .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
      .UseDynamicRendering = true,
      .PipelineRenderingCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
          .colorAttachmentCount = 1,
          .pColorAttachmentFormats = &_swapchainImageFormat,
      },
  };
  ImGui_ImplVulkan_Init(&init_info);
  ImGui_ImplVulkan_CreateFontsTexture();

  _mainDeletionQueue.push_function([=, this]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(_device, imguiPool, nullptr);
  });
};

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView image) {
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
      image, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo =
      vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);
  vkCmdBeginRendering(cmd, &renderInfo);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}

void VulkanEngine::init_pipelines() { init_background_pipeline(); }

void VulkanEngine::init_background_pipeline() {
  VkPipelineLayoutCreateInfo computeLayout{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .setLayoutCount = 1,
      .pSetLayouts = &_drawImageDescriptorLayout,
  };
  VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr,
                                  &_gradientPipelineLayout));

  VkShaderModule computeDrawShader;

  auto shaderPath = (engine_constant::GetShaderRoot() / "gradient.spv");
  if (!vkutil::load_shader_module(_device, shaderPath.c_str(),
                                  &computeDrawShader)) {
    fmt::print(stderr, "Error when building the compute shader, Can't load "
                       "gradient.spv \n");
  }

  VkPipelineShaderStageCreateInfo stageinfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = nullptr,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = computeDrawShader,
      .pName = "main"};

  VkComputePipelineCreateInfo computePipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .stage = stageinfo,
      .layout = _gradientPipelineLayout,
  };
  VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr,
                                    &_gradientPipeline));
  vkDestroyShaderModule(_device, computeDrawShader, nullptr);
  _mainDeletionQueue.push_function([=, this]() {
    vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
    vkDestroyPipeline(_device, _gradientPipeline, nullptr);
  });
}

void VulkanEngine::init_descriptors() {
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes{
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 1}};

  _globalDescriptorAllocator.init_pool(_device, 10, sizes);

  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _drawImageDescriptorLayout =
        builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
  }
  _drawImageDescriptors =
      _globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

  VkDescriptorImageInfo imgInfo{
      .imageView = _drawImage.imageView,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };

  VkWriteDescriptorSet drawImageWrite{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = nullptr,
      .dstSet = _drawImageDescriptors,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .pImageInfo = &imgInfo,
  };
  vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

  _mainDeletionQueue.push_function([=, this]() {
    _globalDescriptorAllocator.destroy_pool(_device);
    vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
  });
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

  RESULT_CHECK(physicalDevice,
               "Can't select suitable PhysicalDevice, errormessage: {}");

  vkb::DeviceBuilder deviceBuilder{*physicalDevice};
  vkb::Result<vkb::Device> vkbDevice = deviceBuilder.build();

  RESULT_CHECK(vkbDevice, "Failed to create physical device, errormessage: {}");

  _device = vkbDevice->device;
  _chosenGPU = vkbDevice->physical_device;

  auto graphicsQueueResult = vkbDevice->get_queue(vkb::QueueType::graphics);
  RESULT_CHECK(graphicsQueueResult,
               "Fail to get graphics queue, errormessage: {}");

  _graphicsQueue = *graphicsQueueResult;

  auto graphicsQueueFamilyResult =
      vkbDevice->get_queue_index(vkb::QueueType::graphics);
  RESULT_CHECK(graphicsQueueFamilyResult,
               "Failed to get Graphics Queue Family, errormessage: {}");

  _graphicsQueueFamily = *graphicsQueueFamilyResult;
  VmaAllocatorCreateInfo allocatorInfo{
      .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
      .physicalDevice = _chosenGPU,
      .device = _device,
      .instance = _instance,
  };
  vmaCreateAllocator(&allocatorInfo, &_allocator);
  _mainDeletionQueue.push_function(
      [this]() { vmaDestroyAllocator(_allocator); });
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
  vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
  _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
  uint32_t minImageCount =
      get_minimum_surface_image_count(_chosenGPU, _surface);
  vkb::Result<vkb::Swapchain> vkbSwapchain =
      swapchainBuilder
          .set_desired_format(VkSurfaceFormatKHR{
              .format = _swapchainImageFormat,
              .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(width, height)
          .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
          .set_desired_min_image_count(minImageCount + 1)
          .build();

  RESULT_CHECK(vkbSwapchain, "Failed to create swapchain, errormessage: {}");
  _swapchainExtent = vkbSwapchain->extent;
  _swapchain = vkbSwapchain->swapchain;

  auto images = vkbSwapchain->get_images();
  RESULT_CHECK(images, "Failed to get image, errormessage: {}");
  _swapchainImage = *images;

  auto image_views = vkbSwapchain->get_image_views();

  RESULT_CHECK(image_views, "Failed to get image view, errormessage: {}");

  _swapchainImageViews = *image_views;
  _frames.resize(_swapchainImage.size());
}

void VulkanEngine::init_swapchain() {
  create_swapchain(_windowExtent.width, _windowExtent.height);
  VkExtent3D drawImageExtent{
      .width = _windowExtent.width,
      .height = _windowExtent.height,
      .depth = 1,
  };
  _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
  _drawImage.imageExtent = drawImageExtent;

  VkImageUsageFlags drawImageUsages{};
  drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo rimg_info = vkinit::image_create_info(
      _drawImage.imageFormat, drawImageUsages, drawImageExtent);
  // GPU VRAM Contain an "Upload Heap" it can be accessed by CPU with PCIE Bus,
  // And that heap size can be control in BIOS or driver panel call "resizable
  // bar".

  // clang-format off
  // So Memory layout like this:
  //                  CPU Ram                                     |               GPU VRAM
  //  Host Only Memory  | Device can access Host Memory | - Connect by PCIE -  | Upload Heap | rest of GPU VRAM
  // clang-format on
  VmaAllocationCreateInfo rimg_allocinfo{
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
  vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image,
                 &_drawImage.allocation, nullptr);
  VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(
      _drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

  VK_CHECK(
      vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));
  _mainDeletionQueue.push_function([=, this]() {
    vkDestroyImageView(_device, _drawImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
  });
}

void VulkanEngine::destory_swapchain() {
  vkDestroySwapchainKHR(_device, _swapchain, nullptr);

  for (auto &imgView : _swapchainImageViews) {
    vkDestroyImageView(_device, imgView, nullptr);
  }
}

void VulkanEngine::init_commands() {
  VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
      _graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  for (int i = 0; i < get_frame_overlap(); i++) {
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr,
                                 &_frames[i]._commandPool));

    VkCommandBufferAllocateInfo cmdAllocInfo =
        vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo,
                                      &_frames[i]._mainCommandBuffer));
  }

  VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr,
                               &_immCommandPool));
  VkCommandBufferAllocateInfo cmdAllocInfo =
      vkinit::command_buffer_allocate_info(_immCommandPool, 1);
  VK_CHECK(
      vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

  _mainDeletionQueue.push_function(
      [=, this]() { vkDestroyCommandPool(_device, _immCommandPool, nullptr); });
}

void VulkanEngine::init_sync_structures() {
  VkFenceCreateInfo fenceInfo =
      vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
  VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();
  for (int i = 0; i < get_frame_overlap(); i++) {
    VK_CHECK(
        vkCreateFence(_device, &fenceInfo, nullptr, &_frames[i]._renderFence));
    VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr,
                               &_frames[i]._renderSemaphore));
    VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr,
                               &_frames[i]._swapchainSemaphore));
  }
  VK_CHECK(vkCreateFence(_device, &fenceInfo, nullptr, &_immFence));
  _mainDeletionQueue.push_function(
      [=, this]() { vkDestroyFence(_device, _immFence, nullptr); });
}

void VulkanEngine::immediate_submit(
    std::function<void(VkCommandBuffer cmd)> &&function) {
  VK_CHECK(vkResetFences(_device, 1, &_immFence));
  VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

  VkCommandBuffer cmd = _immCommandBuffer;
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
  function(cmd);
  VK_CHECK(vkEndCommandBuffer(cmd));
  VkCommandBufferSubmitInfo cmdSubmitInfo =
      vkinit::command_buffer_submit_info(cmd);
  VkSubmitInfo2 submitInfo =
      vkinit::submit_info(&cmdSubmitInfo, nullptr, nullptr);

  VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, _immFence));
  VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
};

void VulkanEngine::cleanup() {
  if (_isInitialized) {
    vkDeviceWaitIdle(_device);
    for (int i = 0; i < get_frame_overlap(); i++) {
      vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
      vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
      vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
      vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
      _frames[i]._deletionQueue.flush();
    }
    _mainDeletionQueue.flush();

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
void VulkanEngine::draw_background(VkCommandBuffer cmd) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          _gradientPipelineLayout, 0, 1, &_drawImageDescriptors,
                          0, nullptr);
  vkCmdDispatch(cmd, std::ceil(float(_drawImage.imageExtent.width) / 16.0f),
                std::ceil(float(_drawImage.imageExtent.height) / 16.0f), 1);
}

void VulkanEngine::draw() {
  VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true,
                           SecondsInNano(1)));
  get_current_frame()._deletionQueue.flush();
  VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

  uint32_t swapchainImageIndex;
  VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, SecondsInNano(1),
                                 get_current_frame()._swapchainSemaphore,
                                 VK_NULL_HANDLE, &swapchainImageIndex));

  VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;
  VK_CHECK(vkResetCommandBuffer(cmd, 0));

  VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  _drawExtent.width = _drawImage.imageExtent.width;
  _drawExtent.height = _drawImage.imageExtent.height;
  VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

  vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_GENERAL);

  // draw command
  draw_background(cmd);

  vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_image(cmd, _swapchainImage[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vkutil::copy_image_to_image(cmd, _drawImage.image,
                              _swapchainImage[swapchainImageIndex], _drawExtent,
                              _swapchainExtent);
  vkutil::transition_image(cmd, _swapchainImage[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);
  vkutil::transition_image(cmd, _swapchainImage[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VK_CHECK(vkEndCommandBuffer(cmd));
  VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
  VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      get_current_frame()._swapchainSemaphore);
  VkSemaphoreSubmitInfo signalInfo =
      vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                    get_current_frame()._renderSemaphore);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);
  VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit,
                          get_current_frame()._renderFence));
  VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &get_current_frame()._renderSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &_swapchain,
      .pImageIndices = &swapchainImageIndex,
  };
  VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));
  _frameNumber++;
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

      ImGui_ImplSDL2_ProcessEvent(&e);
    }

    // do not draw if we are minimized
    if (stop_rendering) {
      // throttle the speed to avoid the endless spinning
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();
    ImGui::Render();

    draw();
  }
}
