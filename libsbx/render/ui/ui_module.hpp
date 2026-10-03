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
#include <libsbx/graphics/resources/sampler.hpp>

#include <libsbx/render/presentation_module.hpp>
#include <libsbx/render/ui_renderer.hpp>
#include <libsbx/render/ui/ui_system.hpp>

namespace sbx::render {

/** @brief Module owning ui_system and registering it with presentation_module. Leaving it out of the module list disables UI. */
class ui_module final : public utility::noncopyable, public ui_renderer {

public:

  using dependencies = core::dependency_list<platform::platform_module, graphics::graphics_module, presentation_module>;

  ui_module();

  ~ui_module();

  auto build_frame() -> ui_draw_data override;

  auto render(graphics::command_buffer& command_buffer, math::vector2u extent, const ui_draw_data& data) -> void override;

  /** @copydoc ui_system::add_layer(Args&&...) */
  template<typename Layer, typename... Args>
  requires (std::is_base_of_v<ui_layer, Layer> && std::is_constructible_v<Layer, Args...>)
  auto add_layer(Args&&... args) -> Layer& {
    return _system.add_layer<Layer>(std::forward<Args>(args)...);
  }

  /** @copydoc ui_system::add_layer(memory::observer_ptr<ui_layer>) */
  auto add_layer(memory::observer_ptr<ui_layer> layer) -> void {
    _system.add_layer(layer);
  }

  /**
   * @brief Unregisters a layer added by pointer.
   *
   * @param layer The layer to remove.
   */
  auto remove_layer(memory::observer_ptr<ui_layer> layer) -> void {
    _system.remove_layer(layer);
  }

  /** @copydoc ui_system::add_font */
  auto add_font(const std::filesystem::path& path, std::float_t size_pixels) -> ImFont* {
    return _system.add_font(path, size_pixels);
  }

  /** @copydoc ui_system::add_default_fonts */
  auto add_default_fonts(std::float_t size_pixels = 16.0f) -> void {
    _system.add_default_fonts(size_pixels);
  }

  /** @copydoc ui_system::apply_default_style */
  auto apply_default_style() -> void {
    _system.apply_default_style();
  }

  /** @copydoc ui_system::texture_id */
  [[nodiscard]] auto texture_id(VkImageView view, VkSampler sampler) -> ImTextureID {
    return _system.texture_id(view, sampler);
  }

  // Owned here so it's destroyed while graphics_module still exists, not at static teardown.
  [[nodiscard]] auto thumbnail_sampler() const noexcept -> VkSampler {
    return _thumbnail_sampler.handle();
  }

private:

  ui_system _system{};
  graphics::sampler _thumbnail_sampler;

}; // class ui_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_UI_MODULE_HPP_
