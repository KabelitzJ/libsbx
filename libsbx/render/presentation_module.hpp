// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PRESENTATION_MODULE_HPP_
#define LIBSBX_RENDER_PRESENTATION_MODULE_HPP_

#include <memory>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/signals/signal.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/render_thread.hpp>
#include <libsbx/render/scene_renderer.hpp>
#include <libsbx/render/ui_renderer.hpp>
#include <libsbx/render/compositor.hpp>

namespace sbx::render {

/**
 * @brief Sole owner of the swapchain frame cycle, driving it through its own render_thread.
 *
 * Scene rendering and UI plug in through the optional scene_renderer and ui_renderer interfaces. prepare()/build_frame() run on the main thread, record()/render() in the kicked work.
 */
class presentation_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<platform::platform_module, graphics::graphics_module>;

  presentation_module();

  ~presentation_module();

  auto render() -> void;

  /**
   * @brief Registers the scene renderer, replacing any previous one.
   *
   * @param renderer The renderer, or nullptr to unregister.
   */
  auto set_scene_renderer(memory::observer_ptr<scene_renderer> renderer) -> void;

  /**
   * @brief Registers the UI renderer, replacing any previous one.
   *
   * @param renderer The renderer, or nullptr to unregister.
   */
  auto set_ui_renderer(memory::observer_ptr<ui_renderer> renderer) -> void;

  /**
   * @brief Sets the compositor; without one the swapchain is cleared.
   *
   * @param compositor The compositor.
   */
  auto set_compositor(std::unique_ptr<compositor> compositor) -> void;

  /**
   * @brief Emitted on the main thread once per frame after the render thread finished, when nothing it records can reference a resource. Use it for changes that would race the render thread.
   *
   * @return The signal.
   */
  [[nodiscard]] auto on_render_idle() noexcept -> signals::signal<>& {
    return _on_render_idle;
  }

private:

  /** @brief The kicked work: runs on the render thread, or inline, depending on threading_policy. */
  auto _consume() -> void;

  std::unique_ptr<render_thread> _render_thread{};

  memory::observer_ptr<scene_renderer> _scene_renderer{};
  memory::observer_ptr<ui_renderer> _ui_renderer{};
  std::unique_ptr<compositor> _compositor{};

  // Built on the main thread by ui_renderer::build_frame(), consumed by ui_renderer::render() in the kicked work.
  ui_draw_data _ui_data{};

  signals::signal<> _on_render_idle{};

}; // class presentation_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_PRESENTATION_MODULE_HPP_
