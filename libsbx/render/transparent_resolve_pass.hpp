// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_TRANSPARENT_RESOLVE_PASS_HPP_
#define LIBSBX_RENDER_TRANSPARENT_RESOLVE_PASS_HPP_

#include <array>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

/**
 * @brief Weighted Blended OIT composite: a fullscreen triangle that resolves
 * transparent_accumulate_pass's accumulator/revealage buffers (average = accumulator.rgb /
 * max(accumulator.a, epsilon), alpha = 1 - revealage) and blends the result "over" the HDR color
 * target already holding opaque_pass/skybox_pass/grid_pass's output. Runs before tonemap_pass,
 * which needs no changes since it already reads the HDR color target afterward.
 */
class transparent_resolve_pass final : public render_pass {

public:

  transparent_resolve_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Transparent Resolve";
  }

  auto execute(render_context& context) -> void override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

}; // class transparent_resolve_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_TRANSPARENT_RESOLVE_PASS_HPP_
