// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_TRANSPARENT_RESOLVE_PASS_HPP_
#define LIBSBX_RENDER_TRANSPARENT_RESOLVE_PASS_HPP_

#include <array>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/** @brief Weighted blended OIT composite: resolves the accumulator/revealage pair (rgb / max(a, epsilon), alpha 1 - revealage) over the HDR color target, before tonemap_pass. */
class transparent_resolve_pass final : public graphics_pass {

public:

  transparent_resolve_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Transparent Resolve";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

}; // class transparent_resolve_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_TRANSPARENT_RESOLVE_PASS_HPP_
