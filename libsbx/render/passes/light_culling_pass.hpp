// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_LIGHT_CULLING_PASS_HPP_
#define LIBSBX_RENDER_LIGHT_CULLING_PASS_HPP_

#include <libsbx/math/vector3.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Clustered Forward+ light culling: partitions the view frustum into a fixed 16x9x24 grid
 * of view-space clusters (shaders/clusters/cluster_data.slang) and assigns every point/spot light to
 * each cluster its bounding sphere overlaps, via two compute dispatches (build_clusters.slang,
 * cull_lights.slang). opaque_pass and transparent_accumulate_pass look up only their own fragment's
 * cluster instead of looping every light in the scene.
 *
 * Directional lights aren't clustered -- they stay in the small, always-evaluated prefix of the
 * light array (render_packet::directional_light_count).
 *
 * Runs right after depth_pre_pass and before opaque_pass: cluster assignment needs only the camera
 * and light list, both already known by then.
 */
class light_culling_pass final : public compute_pass {

public:

  inline static constexpr auto cluster_dimensions = math::vector3u{16u, 9u, 24u};
  inline static constexpr auto cluster_count = cluster_dimensions.x() * cluster_dimensions.y() * cluster_dimensions.z();

  light_culling_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Light Culling";
  }

  auto declare(compute_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context) -> void override;

private:

  memory::observer_ptr<graphics::compute_pipeline> _build_clusters_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _cull_lights_pipeline{};

}; // class light_culling_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_LIGHT_CULLING_PASS_HPP_
