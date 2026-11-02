// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_FRUSTUM_CULL_PASS_HPP_
#define LIBSBX_RENDER_FRUSTUM_CULL_PASS_HPP_

#include <vector>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief GPU frustum culling, one dispatch per command and one thread per instance: opaque commands against the camera (view 0), shadow casters against each shadow view (view 1 + s: the sun's cascades, then local light tiles; see cull_view_count).
 *
 * Visible transforms are compacted per command and instanceCount is bumped atomically; depth_pre_pass and opaque_pass then draw indirectly.
 * Transparent commands aren't culled. Runs after skin_pass, before depth_pre_pass.
 *
 * With occlusion culling on, the camera view is culled twice.
 * The early instance only passes what was visible last frame, which the early depth pre-pass draws and hiz_pass reduces into a depth pyramid.
 * The late instance tests every camera instance against that pyramid, records which are visible for next frame, and passes the newly revealed ones into the late camera view.
 */
class frustum_cull_pass final : public compute_pass {

public:

  /**
   * @brief Creates the early (every view) or late (the camera's occlusion test) cull.
   *
   * @param late Whether this is the late camera cull, which only runs with occlusion culling on.
   */
  explicit frustum_cull_pass(bool late = false);

  [[nodiscard]] auto name() const -> std::string_view override {
    return _late ? "Occlusion Cull" : "Frustum Cull";
  }

  auto declare(compute_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context) const -> bool override {
    return !_late || context.occlusion_enabled;
  }

private:

  auto _cull_view(render_context& context, const std::vector<draw_command>& commands, std::uint32_t cascade_index, std::uint32_t occlusion_mode) -> void;

  memory::observer_ptr<graphics::compute_pipeline> _pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _instanced_pipeline{};

  bool _late;

}; // class frustum_cull_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_FRUSTUM_CULL_PASS_HPP_
