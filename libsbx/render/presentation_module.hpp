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
 * @brief Sole owner of the swapchain frame cycle. Knows about neither 3D scene rendering nor
 * ImGui by type — only two small, symmetric, optional interfaces (scene_renderer, ui_renderer),
 * each independently registered by whichever module implements it (scene_renderer_module,
 * ui_module), each independently present or absent depending on whether that module is in an
 * app's module_list at all. This is what lets `launcher` get a window + swapchain + ImGui with
 * zero 3D rendering machinery ever constructed, and `runtime` get a window + swapchain + a
 * rendered scene with zero ImGui ever constructed.
 *
 * Drives the frame loop via its own render_thread (see render_thread.hpp), whose threading
 * policy comes from core::engine::config() — same mechanism render_module used to own directly.
 * scene_renderer::prepare() (main thread — the only place ECS access is safe) and
 * ui_renderer::build_frame() (main thread) both run before the frame is kicked off;
 * scene_renderer::record() and ui_renderer::render() both run as part of the kicked work, which
 * runs on a separate render thread or inline depending on threading_policy.
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
  // ui_renderer::render() (kicked work, in _consume()) — same "stash between the two phases"
  // shape render_module's own _work_packet member already uses for the scene side.
  ui_draw_data _ui_data{};

}; // class presentation_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_PRESENTATION_MODULE_HPP_
