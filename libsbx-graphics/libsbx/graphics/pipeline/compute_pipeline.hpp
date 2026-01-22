// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_COMPUTE_PIPELINE_HPP_
#define LIBSBX_GRAPHICS_COMPUTE_PIPELINE_HPP_

#include <vector>

#include <vulkan/vulkan.hpp>

#include <fmt/format.h>

#include <libsbx/math/vector3.hpp>

#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>

namespace sbx::graphics {

class compute_pipeline : public pipeline {

  using base = pipeline;

public:

  compute_pipeline(const std::filesystem::path& path);

  ~compute_pipeline() override;

  auto handle() const noexcept -> VkPipeline override {
    return _handle;
  }

  auto has_variable_descriptors() const noexcept -> bool override {
    return false;
  }

  auto descriptor_counts(std::uint32_t set) const noexcept -> std::vector<std::uint32_t> override {
    auto counts = std::vector<std::uint32_t>{};

    for (const auto& binding_data : _set_data[set].binding_data) {
      counts.push_back(binding_data.descriptor_count);
    }

    return counts;
  }

  auto descriptor_set_layout(std::uint32_t set) const noexcept -> VkDescriptorSetLayout override {
    return _set_data[set].layout;
  }

  auto descriptor_pool() const noexcept -> VkDescriptorPool override {
    return _descriptor_pool;
  }

  auto layout() const noexcept -> VkPipelineLayout override {
    return _layout;
  }

  auto bind_point() const noexcept -> VkPipelineBindPoint override {
    return _bind_point;
  }

  auto descriptor_block(const std::string& name, std::uint32_t set) const -> const shader::uniform_block& override {
    if (auto it = _set_data[set].uniform_blocks.find(name); it != _set_data[set].uniform_blocks.end()) {
      return it->second;
    }

    throw std::runtime_error(fmt::format("Failed to find descriptor block '{}' in graphics pipeline '{}'", name, _name));
  }

  auto push_constant() const noexcept -> const std::optional<shader::uniform_block>& override {
    return _push_constant;
  }

  auto find_descriptor_binding(const std::string& name, std::uint32_t set) const -> std::optional<std::uint32_t> override {
    if (auto it = _set_data[set].descriptor_bindings.find(name); it != _set_data[set].descriptor_bindings.end()) {
      return it->second;
    }

    return std::nullopt;
  }

  auto find_descriptor_type_at_binding(std::uint32_t set, std::uint32_t binding) const -> std::optional<VkDescriptorType> override {
    if (_set_data[set].binding_data.size() <= binding) {
      return std::nullopt;
    }

    return _set_data[set].binding_data[binding].descriptor_type;
  }

  auto dispatch(command_buffer& command_buffer, const math::vector3u& groups) -> void {
    vkCmdDispatch(command_buffer, groups.x(), groups.y(), groups.z());
  }

private:

  struct per_binding_data {
    VkDescriptorType descriptor_type;
    std::uint32_t descriptor_count;
  }; // struct per_binding_data

  struct per_set_data {
    std::unordered_map<std::string, shader::uniform> uniforms;
    std::unordered_map<std::string, shader::uniform_block> uniform_blocks;
    std::unordered_map<std::string, std::uint32_t> descriptor_bindings;
    std::unordered_map<std::string, std::uint32_t> descriptor_sizes;
    std::vector<per_binding_data> binding_data;
    VkDescriptorSetLayout layout;
  }; // struct per_set_data

  auto _get_stage_from_name(const std::string& name) const noexcept -> VkShaderStageFlagBits;

  std::unique_ptr<shader> _shader;

  std::vector<per_set_data> _set_data;
  std::optional<shader::uniform_block> _push_constant;

  std::string _name;
  VkPipelineLayout _layout;
  VkPipeline _handle;
  VkPipelineBindPoint _bind_point;

  VkDescriptorPool _descriptor_pool;

}; // class compute_pipeline

using compute_pipeline_handle = resource_handle<compute_pipeline>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_COMPUTE_PIPELINE_HPP_
