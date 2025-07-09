#pragma once 
#include <vk_types.h>

namespace vkutil {

  bool load_shader_module(VkDevice device, std::string_view filePath, VkShaderModule* outShaderModule);

};
