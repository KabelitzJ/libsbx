// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/shader.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

shader::shader(shader_compiler& compiler, const std::filesystem::path& path, std::span<const shader_compiler::entry_point_request> entry_points) {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  auto compiled = compiler.compile(path, entry_points);

  _stages.reserve(compiled.size());

  for (auto& entry : compiled) {
    auto module_create_info = VkShaderModuleCreateInfo{};
    module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_create_info.codeSize = entry.spirv.size() * sizeof(std::uint32_t);
    module_create_info.pCode = entry.spirv.data();

    auto module = VkShaderModule{};
    validate(vkCreateShaderModule(logical_device, &module_create_info, nullptr, &module), "vkCreateShaderModule");

    logical_device.set_debug_name(module, fmt::format("{} [{}]", path.filename().string(), entry.name));

    _stages.push_back(stage{entry.stage, module, std::move(entry.name)});
  }
}

shader::~shader() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  for (const auto& stage : _stages) {
    vkDestroyShaderModule(logical_device, stage.module, nullptr);
  }
}

auto shader::stage_create_infos() const -> std::vector<VkPipelineShaderStageCreateInfo> {
  auto infos = std::vector<VkPipelineShaderStageCreateInfo>{};
  infos.reserve(_stages.size());

  for (const auto& stage : _stages) {
    auto info = VkPipelineShaderStageCreateInfo{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage.stage;
    info.module = stage.module;
    info.pName = stage.entry_point.c_str();

    infos.push_back(info);
  }

  return infos;
}

} // namespace sbx::graphics
