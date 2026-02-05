// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DRAW_LIST_HPP_
#define LIBSBX_GRAPHICS_DRAW_LIST_HPP_

#include <cstddef>
#include <cstdint>

#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/memory/tracking_allocator.hpp>

#include <libsbx/graphics/buffers/storage_buffer.hpp>
#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>
#include <libsbx/graphics/images/separate_sampler_array.hpp>

namespace sbx::graphics {

struct draw_command_range {
  std::uint32_t offset;
  std::uint32_t count;
}; // struct draw_command_range

class draw_list {

public:

  using storage_buffer_container = memory::unordered_map<std::size_t, storage_buffer_handle>;
  using draw_command_range_container = memory::unordered_map<math::uuid, draw_command_range>;

  draw_list() = default;

  virtual ~draw_list();

  virtual auto update() -> void = 0;

  auto buffers() const noexcept -> const storage_buffer_container&;

  auto buffer(const utility::hashed_string& name) const -> const storage_buffer&;

  auto images() const noexcept -> const separate_image2d_array&;

  auto samplers() const noexcept -> const separate_sampler_array&;

  auto draw_ranges(const utility::hashed_string& name) const noexcept -> const draw_command_range_container&;

  auto draw_ranges(const std::size_t hash) const noexcept -> const draw_command_range_container&;

  auto clear() -> void;

  auto create_buffer(const utility::hashed_string& name, VkDeviceSize size, VkBufferUsageFlags additional_usage = 0) -> void;
  
  auto destroy_buffer(const utility::hashed_string& name) -> void;

  template<typename Type>
  auto update_buffer(const memory::vector<Type>& buffer, const utility::hashed_string& name) -> void;

protected:

  auto get_buffer(const utility::hashed_string& name) -> storage_buffer&;

  auto get_buffer(const utility::hashed_string& name) const -> const storage_buffer&;

  auto add_image(const image2d_handle& handle) -> std::uint32_t;

  auto add_sampler_state(const sampler_state_handle& handle) -> std::uint32_t;

  auto push_draw_command_range(const utility::hashed_string& name, const math::uuid& id, const draw_command_range& range) -> void;

  auto push_draw_command_range(const std::size_t hash, const math::uuid& id, const draw_command_range& range) -> void;

private:

  storage_buffer_container _buffers;
  separate_image2d_array _images;
  separate_sampler_array _samplers;
  memory::unordered_map<std::size_t, draw_command_range_container> _draw_ranges;

}; // class draw_list

} // namespace sbx::graphics

#include <libsbx/graphics/draw_list.ipp>

#endif // LIBSBX_GRAPHICS_DRAW_LIST_HPP_
