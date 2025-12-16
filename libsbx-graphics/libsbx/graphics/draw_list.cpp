#include <libsbx/graphics/draw_list.hpp>

#include <libsbx/utility/exception.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

draw_list::~draw_list() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  for (const auto& [name, handle] : _buffers) {
    graphics_module.remove_resource(handle);
  }

  _buffers.clear();

  clear();
}

auto draw_list::buffers() const noexcept -> const storage_buffer_container& {
  return _buffers;
}

auto draw_list::buffer(const utility::hashed_string& name) const -> const storage_buffer& {
  return get_buffer(name);
}

auto draw_list::images() const noexcept -> const separate_image2d_array& {
  return _images;
}

auto draw_list::samplers() const noexcept -> const separate_sampler_array& {
  return _samplers;
}

auto draw_list::draw_ranges(const utility::hashed_string& name) const noexcept -> const draw_command_range_container& {
  static const auto placeholder = draw_command_range_container{};

  if (const auto entry = _draw_ranges.find(name.hash()); entry != _draw_ranges.end()) {
    return entry->second;
  }

  return placeholder;
}

auto draw_list::draw_ranges(const std::size_t hash) const noexcept -> const draw_command_range_container& {
  static const auto placeholder = draw_command_range_container{};

  if (const auto entry = _draw_ranges.find(hash); entry != _draw_ranges.end()) {
    return entry->second;
  }

  return placeholder;
}

auto draw_list::clear() -> void {
  for (auto& [name, range] : _draw_ranges) {
    range.clear();
  }

  _images.clear();
  _samplers.clear();
}

auto draw_list::create_buffer(const utility::hashed_string& name, VkDeviceSize size, VkBufferUsageFlags additional_usage) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  
  _buffers.emplace(name, graphics_module.add_resource<graphics::storage_buffer>(size, additional_usage));
}

auto draw_list::destroy_buffer(const utility::hashed_string& name) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  
  graphics_module.remove_resource<graphics::storage_buffer>(_buffers.at(name));

  _buffers.erase(name);
}

auto draw_list::get_buffer(const utility::hashed_string& name) -> storage_buffer& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (const auto entry = _buffers.find(name.hash()); entry != _buffers.end()) {
    return graphics_module.get_resource<storage_buffer>(entry->second);
  }

  throw std::runtime_error{fmt::format("draw_list::get_buffer: buffer '{}' not found", name.str())};
}

auto draw_list::get_buffer(const utility::hashed_string& name) const -> const storage_buffer& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (const auto entry = _buffers.find(name.hash()); entry != _buffers.end()) {
    return graphics_module.get_resource<storage_buffer>(entry->second);
  }

  throw std::runtime_error{fmt::format("draw_list::get_buffer: buffer '{}' not found", name.str())};
}

auto draw_list::add_image(const image2d_handle& handle) -> std::uint32_t {
  return _images.push_back(handle);
}

auto draw_list::add_sampler_state(const sampler_state_handle& handle) -> std::uint32_t {
  return _samplers.push_back(handle);
}

auto draw_list::push_draw_command_range(const utility::hashed_string& name, const math::uuid& id, const draw_command_range& range) -> void {
  _draw_ranges[name.hash()].emplace(id, range);
}

auto draw_list::push_draw_command_range(const std::size_t hash, const math::uuid& id, const draw_command_range& range) -> void {
  _draw_ranges[hash].emplace(id, range);
}

} // namespace sbx::graphics
