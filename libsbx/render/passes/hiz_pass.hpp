// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_HIZ_PASS_HPP_
#define LIBSBX_RENDER_HIZ_PASS_HPP_

#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Reduces the early depth pre-pass's scene depth into a farthest-depth pyramid (hiz_width x hiz_height, hiz_mip_count levels) for the late occlusion cull.
 *
 * Runs only with occlusion culling on; the pyramid stays in the general layout, written per level through storage views and read through one sampled view.
 */
class hiz_pass final : public compute_pass {

public:

  hiz_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "HiZ";
  }

  auto declare(compute_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context) const -> bool override {
    return context.occlusion_enabled;
  }

private:

  memory::observer_ptr<graphics::compute_pipeline> _pipeline{};

}; // class hiz_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_HIZ_PASS_HPP_
