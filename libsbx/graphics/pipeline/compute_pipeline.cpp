// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/utility/assert.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

compute_pipeline::compute_pipeline(const create_info& create_info) {
  utility::assert_that(create_info.shader != nullptr, "compute_pipeline requires a shader");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
  const auto& bindless_table = graphics_module.bindless_table();

  const auto stages = create_info.shader->stage_create_infos();

  utility::assert_that(stages.size() == 1u, "compute_pipeline requires exactly one shader stage");

  auto pipeline_create_info = VkComputePipelineCreateInfo{};
  pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_create_info.stage = stages.front();
  pipeline_create_info.layout = bindless_table.pipeline_layout();
  pipeline_create_info.basePipelineIndex = -1;
  pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

  validate(vkCreateComputePipelines(logical_device, VK_NULL_HANDLE, 1u, &pipeline_create_info, nullptr, &_handle), "vkCreateComputePipelines");

  logical_device.set_debug_name(_handle, create_info.name);
}

compute_pipeline::~compute_pipeline() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  vkDestroyPipeline(graphics_module.logical_device(), _handle, nullptr);
}

} // namespace sbx::graphics
