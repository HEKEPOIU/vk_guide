#include <fstream>
#include <vk_pipelines.h>

bool vkutil::load_shader_module(VkDevice device, const char *filePath,
                                VkShaderModule *outShaderModule) {
  std::ifstream file(filePath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    return false;
  }

  size_t fileSize = (size_t)file.tellg();

  std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
  file.seekg(0);

  file.read((char *)buffer.data(), fileSize);
  file.close();

  VkShaderModuleCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .codeSize = buffer.size() * sizeof(uint32_t),
      .pCode = buffer.data(),
  };

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    return false;
  }

  *outShaderModule = shaderModule;
  return true;
}
