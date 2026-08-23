// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_GRID_PASS_HPP_
#define LIBSBX_RENDER_GRID_PASS_HPP_

#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

/**
 * @brief World-space reference grid (Blender/Godot-style), editor-only — see
 * render_module::set_grid_enabled. A no-op every frame render_context::show_grid is false, so
 * it's always present in render_module's pass list (demo never enables it).
 *
 * Runs between skybox_pass and transparent_pass, for the same reason skybox_pass does (see its
 * doc comment): it needs the fully-populated opaque depth buffer to be occluded by real geometry,
 * and it must be part of what transparent_pass alpha-blends against.
 */
class grid_pass final : public render_pass {

public:

  grid_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Grid";
  }

  auto execute(render_context& context) -> void override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

}; // class grid_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_GRID_PASS_HPP_
