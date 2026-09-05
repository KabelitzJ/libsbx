// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_UI_MODULE_HPP_
#define LIBSBX_RENDER_UI_UI_MODULE_HPP_

#include <cstdint>
#include <filesystem>
#include <type_traits>
#include <utility>

#include <imgui.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/render/presentation_module.hpp>
#include <libsbx/render/ui_renderer.hpp>
#include <libsbx/render/ui/ui_system.hpp>

namespace sbx::render {

/**
 * @brief Thin core::module owning ui_system, registering itself with presentation_module in its
 * own constructor.
 *
 * Not being in an app's module_list *is* "UI disabled" -- there's no separate flag to track.
 */
class ui_module final : public utility::noncopyable, public ui_renderer {

public:

  using dependencies = core::dependency_list<platform::platform_module, graphics::graphics_module, presentation_module>;

  ui_module();

  ~ui_module();

  auto build_frame() -> ui_draw_data override;

  auto render(graphics::command_buffer& command_buffer, math::vector2u extent, const ui_draw_data& data) -> void override;

  /** @ref ui_system::add_layer */
  template<typename Layer, typename... Args>
  requires (std::is_base_of_v<ui_layer, Layer> && std::is_constructible_v<Layer, Args...>)
  auto add_layer(Args&&... args) -> Layer& {
    return _system.add_layer<Layer>(std::forward<Args>(args)...);
  }

  /** @ref ui_system::add_layer */
  auto add_layer(memory::observer_ptr<ui_layer> layer) -> void {
    _system.add_layer(layer);
  }

  /** @ref ui_system::remove_layer */
  auto remove_layer(memory::observer_ptr<ui_layer> layer) -> void {
    _system.remove_layer(layer);
  }

  /** @ref ui_system::add_font */
  auto add_font(const std::filesystem::path& path, std::float_t size_pixels) -> ImFont* {
    return _system.add_font(path, size_pixels);
  }

  /** @ref ui_system::add_default_fonts */
  auto add_default_fonts(std::float_t size_pixels = 16.0f) -> void {
    _system.add_default_fonts(size_pixels);
  }

  /** @ref ui_system::apply_default_style */
  auto apply_default_style() -> void {
    _system.apply_default_style();
  }

  /** @ref ui_system::texture_id */
  [[nodiscard]] auto texture_id(VkImageView view, VkSampler sampler) -> ImTextureID {
    return _system.texture_id(view, sampler);
  }

private:

  ui_system _system{};

}; // class ui_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_UI_MODULE_HPP_
