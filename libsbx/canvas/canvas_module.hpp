// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CANVAS_CANVAS_MODULE_HPP_
#define LIBSBX_CANVAS_CANVAS_MODULE_HPP_

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/canvas/components.hpp>
#include <libsbx/canvas/rect_resolve.hpp>
#include <libsbx/canvas/canvas_draw_list.hpp>

namespace sbx::canvas {

/**
 * @brief Owns and hit-tests every canvas/UI-element hierarchy in the active scene, and bakes this
 * frame's resolved geometry into a canvas_draw_list for canvas_pass to draw. Picked up
 * automatically as the core::stage::update hook (see core::module's reflection-based dispatch).
 *
 * v1 scope: only render_mode::screen_space_overlay is functional (the canvas fills the whole
 * window); screen_space_camera/world_space are reserved enum values, not yet implemented -- both
 * would need a depth-tested per-camera sub-pass canvas_pass doesn't have. Only Panel/Image/Button
 * actually render; ui_text is authored but not drawn (no font-atlas pipeline yet -- see
 * components.hpp). No layout groups or auto-sizing -- every rect_transform is hand-placed.
 *
 * Hit-testing is pull-based, not push-based: any code doing world picking (a road tool, editor
 * viewport picking) should call wants_pointer_capture() before casting its own ray, so a click on
 * the UI never falls through to the world underneath it.
 */
class canvas_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<scenes::scenes_module, platform::platform_module, assets::assets_module>;

  auto update() -> void;

  [[nodiscard]] auto wants_pointer_capture() const noexcept -> bool {
    return _wants_pointer_capture;
  }

  [[nodiscard]] auto draw_list() noexcept -> canvas_draw_list& {
    return _draw_list;
  }

private:

  // node is taken by value (a cheap registry-ptr + entity handle) rather than const-ref, since
  // ui_button's hover/press/click state must be written through get_component's mutable overload.
  auto _visit(scenes::scene& scene, scenes::node node, const resolved_rect& parent_rect, const math::vector2& screen_size, const math::vector2& mouse_position) -> void;

  canvas_draw_list _draw_list{};
  bool _wants_pointer_capture{false};

}; // class canvas_module

} // namespace sbx::canvas

#endif // LIBSBX_CANVAS_CANVAS_MODULE_HPP_
