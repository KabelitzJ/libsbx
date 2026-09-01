// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <algorithm>
#include <array>
#include <vector>

#include <libsbx/utility/assert.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

graphics_pipeline::graphics_pipeline(const create_info& create_info) {
  utility::assert_that(create_info.shader != nullptr, "graphics_pipeline requires a shader");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
  const auto& physical_device = graphics_module.physical_device();
  const auto& bindless_table = graphics_module.bindless_table();
  const auto& pipeline_binary_cache = graphics_module.pipeline_binary_cache();

  auto stages = create_info.shader->stage_create_infos();

  auto specialization_map_entries = std::vector<VkSpecializationMapEntry>{};
  auto specialization_data = std::vector<std::uint32_t>{};
  auto specialization_info = VkSpecializationInfo{};

  if (!create_info.specialization_constants.empty()) {
    specialization_map_entries.reserve(create_info.specialization_constants.size());
    specialization_data.reserve(create_info.specialization_constants.size());

    for (const auto& constant : create_info.specialization_constants) {
      specialization_data.push_back(constant.value);

      specialization_map_entries.push_back(VkSpecializationMapEntry{
        constant.constant_id,
        static_cast<std::uint32_t>((specialization_data.size() - 1u) * sizeof(std::uint32_t)),
        sizeof(std::uint32_t)
      });
    }

    specialization_info.mapEntryCount = static_cast<std::uint32_t>(specialization_map_entries.size());
    specialization_info.pMapEntries = specialization_map_entries.data();
    specialization_info.dataSize = specialization_data.size() * sizeof(std::uint32_t);
    specialization_info.pData = specialization_data.data();

    for (auto& stage : stages) {
      stage.pSpecializationInfo = &specialization_info;
    }
  }

  auto vertex_input_state = VkPipelineVertexInputStateCreateInfo{};
  vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  auto input_assembly_state = VkPipelineInputAssemblyStateCreateInfo{};
  input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_state.topology = to_vk_enum<VkPrimitiveTopology>(create_info.topology);
  input_assembly_state.primitiveRestartEnable = create_info.primitive_restart ? VK_TRUE : VK_FALSE;

  auto viewport_state = VkPipelineViewportStateCreateInfo{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1u;
  viewport_state.scissorCount = 1u;

  auto rasterization_state = VkPipelineRasterizationStateCreateInfo{};
  rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization_state.polygonMode = to_vk_enum<VkPolygonMode>(create_info.polygon_mode);
  rasterization_state.cullMode = static_cast<VkCullModeFlags>(create_info.cull_mode);
  rasterization_state.frontFace = to_vk_enum<VkFrontFace>(create_info.front_face);

  const auto& line_width_range = physical_device.properties().limits.lineWidthRange;
  rasterization_state.lineWidth = std::clamp(create_info.line_width, line_width_range[0], line_width_range[1]);

  if (create_info.depth_bias.has_value()) {
    rasterization_state.depthBiasEnable = VK_TRUE;
    rasterization_state.depthBiasConstantFactor = create_info.depth_bias->constant_factor;
    rasterization_state.depthBiasSlopeFactor = create_info.depth_bias->slope_factor;
    rasterization_state.depthBiasClamp = create_info.depth_bias->clamp;
  }

  auto multisample_state = VkPipelineMultisampleStateCreateInfo{};
  multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample_state.rasterizationSamples = to_vk_enum<VkSampleCountFlagBits>(create_info.samples);

  auto depth_stencil_state = VkPipelineDepthStencilStateCreateInfo{};
  depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state.depthTestEnable = create_info.depth_test ? VK_TRUE : VK_FALSE;
  depth_stencil_state.depthWriteEnable = create_info.depth_write ? VK_TRUE : VK_FALSE;
  depth_stencil_state.depthCompareOp = to_vk_enum<VkCompareOp>(create_info.depth_compare);

  auto color_blend_attachments = std::vector<VkPipelineColorBlendAttachmentState>{};
  color_blend_attachments.reserve(create_info.color_formats.size());

  for (auto index = std::size_t{0u}; index < create_info.color_formats.size(); ++index) {
    const auto blend = (index < create_info.color_blend_attachments.size()) ? create_info.color_blend_attachments[index] : blend_attachment{};

    auto attachment = VkPipelineColorBlendAttachmentState{};
    attachment.blendEnable = blend.enable ? VK_TRUE : VK_FALSE;
    attachment.srcColorBlendFactor = to_vk_enum<VkBlendFactor>(blend.source_color);
    attachment.dstColorBlendFactor = to_vk_enum<VkBlendFactor>(blend.destination_color);
    attachment.colorBlendOp = to_vk_enum<VkBlendOp>(blend.color_operation);
    attachment.srcAlphaBlendFactor = to_vk_enum<VkBlendFactor>(blend.source_alpha);
    attachment.dstAlphaBlendFactor = to_vk_enum<VkBlendFactor>(blend.destination_alpha);
    attachment.alphaBlendOp = to_vk_enum<VkBlendOp>(blend.alpha_operation);
    attachment.colorWriteMask = reflection::to_underlying(blend.color_write_mask);

    color_blend_attachments.push_back(attachment);
  }

  auto color_blend_state = VkPipelineColorBlendStateCreateInfo{};
  color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend_state.attachmentCount = static_cast<std::uint32_t>(color_blend_attachments.size());
  color_blend_state.pAttachments = color_blend_attachments.data();

  const auto dynamic_states = std::array<VkDynamicState, 2u>{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  auto dynamic_state = VkPipelineDynamicStateCreateInfo{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  auto color_formats = std::vector<VkFormat>{};
  color_formats.reserve(create_info.color_formats.size());

  for (const auto format : create_info.color_formats) {
    color_formats.push_back(to_vk_enum<VkFormat>(format));
  }

  auto rendering_info = VkPipelineRenderingCreateInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering_info.colorAttachmentCount = static_cast<std::uint32_t>(color_formats.size());
  rendering_info.pColorAttachmentFormats = color_formats.data();

  if (create_info.depth_format != format::undefined) {
    rendering_info.depthAttachmentFormat = to_vk_enum<VkFormat>(create_info.depth_format);
  }

  auto pipeline_create_info = VkGraphicsPipelineCreateInfo{};
  pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_create_info.pNext = &rendering_info;
  pipeline_create_info.stageCount = static_cast<std::uint32_t>(stages.size());
  pipeline_create_info.pStages = stages.data();
  pipeline_create_info.pVertexInputState = &vertex_input_state;
  pipeline_create_info.pInputAssemblyState = &input_assembly_state;
  pipeline_create_info.pViewportState = &viewport_state;
  pipeline_create_info.pRasterizationState = &rasterization_state;
  pipeline_create_info.pMultisampleState = &multisample_state;
  pipeline_create_info.pDepthStencilState = &depth_stencil_state;
  pipeline_create_info.pColorBlendState = &color_blend_state;
  pipeline_create_info.pDynamicState = &dynamic_state;
  pipeline_create_info.layout = bindless_table.pipeline_layout();

  validate(vkCreateGraphicsPipelines(logical_device, pipeline_binary_cache, 1u, &pipeline_create_info, nullptr, &_handle), "vkCreateGraphicsPipelines");

  logical_device.set_debug_name(_handle, create_info.name);
}

graphics_pipeline::~graphics_pipeline() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  vkDestroyPipeline(graphics_module.logical_device(), _handle, nullptr);
}

} // namespace sbx::graphics
