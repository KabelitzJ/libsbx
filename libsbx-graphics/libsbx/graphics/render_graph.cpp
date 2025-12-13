#include <libsbx/graphics/render_graph.hpp>

#include <queue>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>

#include <libsbx/math/color.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/descriptor/descriptor.hpp>

namespace sbx::graphics {

attachment::attachment(const utility::hashed_string& name, type type, const math::color& clear_color, const graphics::format format, const graphics::blend_state& blend_state, const graphics::address_mode address_mode) noexcept
: _name{name}, 
  _type{type},
  _clear_color{clear_color},
  _format{format}, 
  _address_mode{address_mode},
  _blend_state{blend_state} { }

attachment::attachment(const utility::hashed_string& name, type type, const math::color& clear_color, const graphics::format format, const graphics::address_mode address_mode) noexcept
: _name{name}, 
  _type{type},
  _clear_color{clear_color},
  _format{format}, 
  _address_mode{address_mode},
  _blend_state{} { }

auto attachment::name() const noexcept -> const utility::hashed_string& {
  return _name;
}

auto attachment::image_type() const noexcept -> type {
  return _type;
}

auto attachment::format() const noexcept -> graphics::format {
  return _format;
}

auto attachment::address_mode() const noexcept -> graphics::address_mode {
  return _address_mode;
}

auto attachment::clear_color() const noexcept -> const math::color& {
  return _clear_color;
}

auto attachment::blend_state() const noexcept -> const graphics::blend_state& {
  return _blend_state;
}

auto render_graph::find_attachment(const std::string& name) const -> const image2d& {
  auto entry = _image_by_name.find(name);

  if (entry == _image_by_name.end()) {
    throw utility::runtime_error{"Render graph does not have attachment '{}", name};
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  return graphics_module.get_resource<image2d>(_images[entry->second]);
}

render_graph::~render_graph() {
  auto& rendering_module = core::engine::get_module<graphics::graphics_module>();

  for (auto& image_handle : _images) {
    rendering_module.remove_resource<image2d>(image_handle);
  }

  for (auto& depth_image_handle : _depth_images) {
    rendering_module.remove_resource<depth_image>(depth_image_handle);
  }
}

} // namespace sbx::graphics
