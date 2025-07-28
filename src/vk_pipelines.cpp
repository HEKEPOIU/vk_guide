#include "vk_initializers.h"
#include <fstream>
#include <vk_pipelines.h>

namespace vkutil {

bool load_shader_module(VkDevice device, std::string_view filePath,
                        VkShaderModule *outShaderModule) {
  std::ifstream file(filePath.data(), std::ios::ate | std::ios::binary);

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

void PipelineBuilder::clear() {
  _shaderStages.clear();
  _inputAssembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  _rasterization = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  _colorBlendAttachment = {};
  _multisample = {.sType =
                      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  _pipelineLayout = {};
  _depthStencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  _renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  _colorAttachmentFormat = {};
}
VkPipeline PipelineBuilder::build_pipeline(VkDevice device) {
  VkPipelineViewportStateCreateInfo viewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .viewportCount = 1,
      .scissorCount = 1};
  VkPipelineColorBlendStateCreateInfo colorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &_colorBlendAttachment,
  };

  VkPipelineVertexInputStateCreateInfo _vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  VkDynamicState dynamicStateEnables[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = uint32_t(std::size(dynamicStateEnables)),
      .pDynamicStates = &dynamicStateEnables[0],
  };
  VkGraphicsPipelineCreateInfo pipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &_renderInfo,
      .stageCount = uint32_t(_shaderStages.size()),
      .pStages = _shaderStages.data(),
      .pVertexInputState = &_vertexInputInfo,
      .pInputAssemblyState = &_inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &_rasterization,
      .pMultisampleState = &_multisample,
      .pDepthStencilState = &_depthStencil,
      .pColorBlendState = &colorBlending,
      .pDynamicState = &dynamicState,
      .layout = _pipelineLayout,
  };
  VkPipeline pipeline;
  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
                                nullptr, &pipeline) != VK_SUCCESS) {
    fmt::println("Failed to create graphics pipeline");
    return VK_NULL_HANDLE;
  }
  return pipeline;
}
void PipelineBuilder::set_shader(VkShaderModule vertexShader,
                                 VkShaderModule fragmentShader) {
  _shaderStages.clear();
  _shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(
      VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
  _shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(
      VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}
void PipelineBuilder::set_input_topology(VkPrimitiveTopology topology) {
  _inputAssembly.topology = topology;
  _inputAssembly.primitiveRestartEnable = VK_FALSE;
}
void PipelineBuilder::set_polygon_mode(VkPolygonMode polygonMode) {
  _rasterization.polygonMode = polygonMode;
  _rasterization.lineWidth = 1.0f;
}
void PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode,
                                    VkFrontFace frontFace) {
  _rasterization.cullMode = cullMode;
  _rasterization.frontFace = frontFace;
}
void PipelineBuilder::set_multisample_none() {
  _multisample.sampleShadingEnable = VK_FALSE;
  _multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  _multisample.minSampleShading = 1.0f;
  _multisample.pSampleMask = nullptr;
  _multisample.alphaToCoverageEnable = VK_FALSE;
  _multisample.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::disable_blending() {
  _colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  _colorBlendAttachment.blendEnable = VK_FALSE;
}
void PipelineBuilder::set_color_attachment_format(VkFormat format) {
  _colorAttachmentFormat = format;
  _renderInfo.colorAttachmentCount = 1;
  _renderInfo.pColorAttachmentFormats = &_colorAttachmentFormat;
  _renderInfo.pNext = nullptr;
}
void PipelineBuilder::set_depth_format(VkFormat format) {
    _renderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::disable_depthtest() {
  _depthStencil.depthTestEnable = VK_FALSE;
  _depthStencil.depthWriteEnable = VK_FALSE; 
  _depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  _depthStencil.depthBoundsTestEnable = VK_FALSE;
  _depthStencil.stencilTestEnable = VK_FALSE;
  _depthStencil.front = {};
  _depthStencil.back = {};
  _depthStencil.minDepthBounds = 0.0f;
  _depthStencil.maxDepthBounds = 1.0f;
}
void PipelineBuilder::enable_depthtest(bool depthWriteEnable, VkCompareOp op) {
  _depthStencil.depthTestEnable = VK_TRUE;
  _depthStencil.depthWriteEnable = depthWriteEnable;
  _depthStencil.depthCompareOp = op;
  _depthStencil.depthBoundsTestEnable = VK_FALSE;
  _depthStencil.stencilTestEnable = VK_FALSE;
  _depthStencil.front = {};
  _depthStencil.back = {};
  _depthStencil.minDepthBounds = 0.0f;
  _depthStencil.maxDepthBounds = 1.0f;
}

void PipelineBuilder::enable_blending_additive() {
// outColor = srcColor * srcColorBlendFactor <Op> dstColor * dstColorBlendFactor
// outColor = srcColor * srcAlpha + dstColor
// outAlpah = srcAlpha + dstAlpha * 0
  _colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  _colorBlendAttachment.blendEnable = VK_TRUE;
  _colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  _colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  _colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  _colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  _colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  _colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enable_blending_alpha_blend() {
// outColor = srcColor * srcColorBlendFactor <Op> dstColor * dstColorBlendFactor
// outColor = srcColor * srcAlpha + dstColor * (1 - srcAlpha)
// outAlpah = srcAlpha + dstAlpha * 0

  _colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  _colorBlendAttachment.blendEnable = VK_TRUE;
  _colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  _colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  _colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  _colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  _colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  _colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

} // namespace vkutil
