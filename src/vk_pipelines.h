#pragma once 
#include <vk_types.h>

namespace vkutil {

  bool load_shader_module(VkDevice device, std::string_view filePath, VkShaderModule* outShaderModule);

  class PipelineBuilder {
    public:
      std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
      VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
      VkPipelineRasterizationStateCreateInfo _rasterization;
      VkPipelineColorBlendAttachmentState _colorBlendAttachment;
      VkPipelineMultisampleStateCreateInfo _multisample;
      VkPipelineLayout _pipelineLayout;
      VkPipelineDepthStencilStateCreateInfo _depthStencil;
      VkPipelineRenderingCreateInfo _renderInfo;
      VkFormat _colorAttachmentFormat;
      PipelineBuilder(){clear();};
      void set_shader(VkShaderModule vertexShader, VkShaderModule fragmentShader);
      void set_input_topology(VkPrimitiveTopology topology);
      void set_polygon_mode(VkPolygonMode polygonMode);
      void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
      void set_multisample_none();
      void disable_blending();
      void set_color_attachment_format(VkFormat format);
      void set_depth_format(VkFormat format);
      void disable_depthtest();
      void clear();
      VkPipeline build_pipeline(VkDevice device);

  };

};
