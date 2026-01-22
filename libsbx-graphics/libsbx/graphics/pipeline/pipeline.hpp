// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_PIPELINE_PIPELINE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_PIPELINE_HPP_

#include <cinttypes>
#include <optional>

#include <vulkan/vulkan.hpp>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/hash.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/pipeline/shader.hpp>

#include <libsbx/graphics/render_graph.hpp>

namespace sbx::graphics {

class pipeline : public utility::noncopyable {

public:

  pipeline() { }

  virtual ~pipeline() = default;

  auto bind(command_buffer& command_buffer) const noexcept -> void {
    vkCmdBindPipeline(command_buffer, bind_point(), handle());
  }

  operator VkPipeline() const noexcept {
    return handle();
  }

  virtual auto handle() const noexcept -> VkPipeline = 0;

  virtual auto has_variable_descriptors() const noexcept -> bool = 0;

  virtual auto descriptor_counts(std::uint32_t set) const noexcept -> std::vector<std::uint32_t> = 0;

  virtual auto descriptor_set_layout(std::uint32_t set) const noexcept -> VkDescriptorSetLayout = 0;

  virtual auto descriptor_pool() const noexcept -> VkDescriptorPool = 0;

  virtual auto layout() const noexcept -> VkPipelineLayout = 0;

  virtual auto bind_point() const noexcept -> VkPipelineBindPoint = 0;

  virtual auto descriptor_block(const std::string& name, std::uint32_t set) const -> const shader::uniform_block& = 0;

  virtual auto push_constant() const noexcept -> const std::optional<shader::uniform_block>& = 0;

  virtual auto find_descriptor_binding(const std::string& name, std::uint32_t set) const -> std::optional<std::uint32_t> = 0;

  virtual auto find_descriptor_type_at_binding(std::uint32_t set, std::uint32_t binding) const -> std::optional<VkDescriptorType> = 0;

}; // class pipeline

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_PIPELINE_HPP_
