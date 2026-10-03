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
 * @brief GPU frustum culling, one dispatch per command and one thread per instance: opaque commands against the camera (view 0), shadow casters against each cascade (view 1 + c).
 *
 * Visible transforms are compacted per command and instanceCount is bumped atomically; depth_pre_pass and opaque_pass then draw indirectly.
 * Transparent commands aren't culled. Runs after skin_pass, before depth_pre_pass.
 */
class frustum_cull_pass final : public compute_pass {

public:

  frustum_cull_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Frustum Cull";
  }

  auto declare(compute_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context) -> void override;

private:

  auto _cull_view(render_context& context, const std::vector<draw_command>& commands, std::uint32_t cascade_index) -> void;

  memory::observer_ptr<graphics::compute_pipeline> _pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _instanced_pipeline{};

}; // class frustum_cull_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_FRUSTUM_CULL_PASS_HPP_
