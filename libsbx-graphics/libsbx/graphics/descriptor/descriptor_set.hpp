// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DESCRIPTOR_DESCRIPTOR_SET_HPP_
#define LIBSBX_GRAPHICS_DESCRIPTOR_DESCRIPTOR_SET_HPP_

#include <vulkan/vulkan.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/pipeline/pipeline.hpp>

namespace sbx::graphics {

class descriptor_set {

public:

  explicit descriptor_set(const pipeline& pipeline, std::uint32_t set) noexcept;

  ~descriptor_set();

  static auto update(const std::vector<VkWriteDescriptorSet>& write_descriptor_sets) -> void;

  auto handle() const noexcept -> VkDescriptorSet;

  operator VkDescriptorSet() const noexcept;

  auto bind(command_buffer& command_buffer) const noexcept -> void;

private:

  std::uint32_t _set;
  VkPipelineLayout _pipeline_layout;
  VkPipelineBindPoint _pipeline_bind_point;
  VkDescriptorPool _descriptor_pool;
  VkDescriptorSet _descriptor_set;

}; // class descriptor_set

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DESCRIPTOR_DESCRIPTOR_SET_HPP_
