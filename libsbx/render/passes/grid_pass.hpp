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

/** @brief Editor world-space reference grid, a no-op unless enabled. Runs between skybox_pass and transparent_accumulate_pass so geometry occludes it. */
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
