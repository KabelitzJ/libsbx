// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PRESENTATION_MODULE_HPP_
#define LIBSBX_RENDER_PRESENTATION_MODULE_HPP_

#include <memory>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/render_thread.hpp>
#include <libsbx/render/scene_renderer.hpp>
#include <libsbx/render/ui_renderer.hpp>
#include <libsbx/render/compositor.hpp>

namespace sbx::render {

/**
 * @brief Sole owner of the swapchain frame cycle.
 *
 * Scene rendering and UI are known only through two optional interfaces (scene_renderer,
 * ui_renderer), each registered independently by whichever module implements it. Drives the frame
 * loop via its own render_thread; prepare()/build_frame() run on the main thread before the frame
 * is kicked, record()/render() run as part of the kicked work.
 */
class presentation_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<platform::platform_module, graphics::graphics_module>;

  presentation_module();

  ~presentation_module();

  auto render() -> void;

  /** @brief At most one. Pass nullptr to unregister. */
  auto set_scene_renderer(memory::observer_ptr<scene_renderer> renderer) -> void;

  /** @brief At most one. Pass nullptr to unregister. */
  auto set_ui_renderer(memory::observer_ptr<ui_renderer> renderer) -> void;

  /** @brief At most one. Unset clears the swapchain instead. */
  auto set_compositor(std::unique_ptr<compositor> compositor) -> void;

private:

  /** @brief The kicked work — runs on the render thread, or inline, depending on threading_policy. */
  auto _consume() -> void;

  std::unique_ptr<render_thread> _render_thread{};

  memory::observer_ptr<scene_renderer> _scene_renderer{};
  memory::observer_ptr<ui_renderer> _ui_renderer{};
  std::unique_ptr<compositor> _compositor{};

  // Built by ui_renderer::build_frame() (main thread, in render()), consumed by
  // ui_renderer::render() (kicked work, in _consume()).
  ui_draw_data _ui_data{};

}; // class presentation_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_PRESENTATION_MODULE_HPP_
