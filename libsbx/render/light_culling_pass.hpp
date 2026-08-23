// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_LIGHT_CULLING_PASS_HPP_
#define LIBSBX_RENDER_LIGHT_CULLING_PASS_HPP_

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

/**
 * @brief Clustered Forward+ light culling: partitions the view frustum into a fixed 16x9x24 grid
 * of view-space clusters (see shaders/pbr/cluster_data.slang) and assigns every point/spot light
 * to each cluster its bounding sphere overlaps, via two compute dispatches
 * (shaders/pbr/build_clusters.slang, shaders/pbr/cull_lights.slang). opaque_pass and
 * transparent_accumulate_pass share the same lighting code (shaders/pbr/geometry.slang's
 * evaluate_lit) and each only look up their own fragment's cluster instead of looping every light
 * in the scene.
 *
 * Directional lights aren't clustered — they're screen-wide, so they stay in the small,
 * always-evaluated prefix of the light array exactly as render_module packs them
 * (render_packet::directional_light_count).
 *
 * Runs right after depth_pre_pass and before opaque_pass: cluster assignment depends only on the
 * camera and the light list, both already known by then, not on anything opaque_pass or
 * transparent_accumulate_pass produce.
 */
class light_culling_pass final : public render_pass {

public:

  light_culling_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Light Culling";
  }

  auto execute(render_context& context) -> void override;

private:

  memory::observer_ptr<graphics::compute_pipeline> _build_clusters_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _cull_lights_pipeline{};

}; // class light_culling_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_LIGHT_CULLING_PASS_HPP_
