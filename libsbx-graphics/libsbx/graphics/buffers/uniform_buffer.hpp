// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_BUFFERS_UNIFORM_BUFFER_HPP_
#define LIBSBX_GRAPHICS_BUFFERS_UNIFORM_BUFFER_HPP_

#include <vulkan/vulkan.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/buffers/buffer.hpp>

#include <libsbx/graphics/descriptor/descriptor.hpp>

#include <libsbx/graphics/resource_storage.hpp>

namespace sbx::graphics {

class uniform_buffer : public buffer, public descriptor {

public:

  uniform_buffer(VkDeviceSize size, memory::observer_ptr<void> data = nullptr);

  ~uniform_buffer() override;

  auto mapped_memory() const noexcept -> memory::observer_ptr<void>;

  auto update(memory::observer_ptr<const void> data, VkDeviceSize size, VkDeviceSize offset = 0) -> void;

  auto write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set override;

  static auto create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags) noexcept -> VkDescriptorSetLayoutBinding;

}; // class uniform_buffer

using uniform_buffer_handle = resource_handle<uniform_buffer>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_BUFFERS_UNIFORM_BUFFER_HPP_
