// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_GRID_PASS_HPP_
#define LIBSBX_RENDER_GRID_PASS_HPP_

#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief World-space reference grid (Blender/Godot-style), editor-only; toggled via
 * @ref scene_renderer_module::set_grid_enabled. A no-op when render_context::show_grid is false, so
 * it stays in the pass list unconditionally.
 *
 * Runs between skybox_pass and transparent_accumulate_pass: needs the finished opaque depth buffer
 * to be occluded by real geometry, and must be part of what transparent_resolve_pass composites
 * against.
 */
class grid_pass final : public graphics_pass {

public:

  grid_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Grid";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context, std::uint32_t group) const -> bool override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

}; // class grid_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_GRID_PASS_HPP_
