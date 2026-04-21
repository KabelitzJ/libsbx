// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_BUFFERS_STORAGE_BUFFER_HPP_
#define LIBSBX_GRAPHICS_BUFFERS_STORAGE_BUFFER_HPP_

#include <libsbx/units/bytes.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/descriptor/descriptor.hpp>

#include <libsbx/graphics/buffers/buffer.hpp>

#include <libsbx/graphics/resource_storage.hpp>

namespace sbx::graphics {

class storage_buffer : public buffer, public descriptor  {

public:

  inline static constexpr auto max_size = units::quantity_cast<units::byte>(units::kibibyte{512});
  inline static constexpr auto min_size = units::quantity_cast<units::byte>(units::kibibyte{16});

  storage_buffer(VkDeviceSize size, memory::observer_ptr<const void> data = nullptr);

  storage_buffer(VkDeviceSize size, VkBufferUsageFlags additional_usage, memory::observer_ptr<const void> data = nullptr);

  ~storage_buffer() override;

  auto update(memory::observer_ptr<const void> data, VkDeviceSize size, std::size_t offset = 0u) -> void;

  template<typename Type>
  auto read(const std::size_t index) const -> const Type& {
    return *(static_cast<const Type*>(_mapped_memory.get()) + index);
  }

  auto name() const noexcept -> std::string override {
    return "Storage Buffer";
  }

  auto write_descriptor_set(std::uint32_t binding, VkDescriptorType descriptor_type) const noexcept -> graphics::write_descriptor_set override;

  static auto create_descriptor_set_layout_binding(std::uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags) noexcept -> VkDescriptorSetLayoutBinding;

}; // class storage_buffer

using storage_buffer_handle = resource_handle<storage_buffer>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_BUFFERS_STORAGE_BUFFER_HPP_

