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
 * @brief GPU-driven frustum culling, one compute dispatch per draw command and one thread per
 * instance: context.packet->opaque_commands against the camera frustum (cull view 0), and, when
 * shadows are on, shadow_caster_commands against each cascade's light frustum (cull view 1 + c). A visible instance's transform_data is
 * compacted (contiguously, per command) into the culled-transforms buffer and that command's
 * VkDrawIndexedIndirectCommand::instanceCount is atomically incremented; depth_pre_pass/opaque_pass
 * then draw via submit_draw_commands_indirect instead of submit_draw_commands, reading the compacted
 * result instead of every instance unconditionally.
 *
 * transparent_commands are not culled. Runs right after skin_pass and before depth_pre_pass --
 * only needs this frame's transform buffer and frame_data (view/projection/frustum_planes), both
 * already written by scene_renderer_module::_prepare_frame before any pass in the graph executes.
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

}; // class frustum_cull_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_FRUSTUM_CULL_PASS_HPP_
