#include <libsbx/ui/atlas.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::ui {

atlas::atlas(std::uint32_t width, std::uint32_t height, const std::vector<std::uint8_t>& data)
: _width{width},
  _height{height} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  _image_id = graphics_module.add_resource<graphics::image2d>(math::vector2u{_width, _height}, graphics::format::r8_unorm, VK_FILTER_LINEAR, data.data());
}

auto atlas::width() const noexcept -> std::uint32_t {
  return _width;
}

auto atlas::height() const noexcept -> std::uint32_t {
  return _height;
}

auto atlas::image() const noexcept -> const graphics::image2d& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  return graphics_module.get_resource<graphics::image2d>(_image_id);
}

} // namespace sbx::ui
