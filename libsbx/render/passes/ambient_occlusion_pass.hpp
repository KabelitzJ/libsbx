// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PASSES_AMBIENT_OCCLUSION_PASS_HPP_
#define LIBSBX_RENDER_PASSES_AMBIENT_OCCLUSION_PASS_HPP_

#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Screen-space ambient occlusion (post_process_settings::ambient_occlusion), at half
 * resolution from depth_pre_pass's resolved depth: view-space position and normal rebuilt from
 * depth, a per-pixel-rotated hemisphere of samples tested against the depth buffer (with a range
 * check, so far-away geometry doesn't darken), then a depth-aware blur. lighting.slang multiplies
 * the ambient term by the result (frame_data::ambient_occlusion_index).
 *
 * The two half-resolution targets are scene_renderer_module's (created/retired with the other
 * targets, indices in render_context). Like bloom_pass it never skips execute(): when AO is off it
 * still moves both targets to shader_read_only_optimal, which opaque_pass's declared read expects;
 * frame_data's index is 0xFFFFFFFF then, so nothing samples the stale contents.
 *
 * Runs right after depth_pre_pass, before opaque_pass.
 */
class ambient_occlusion_pass final : public compute_pass {

public:

  ambient_occlusion_pass();

  ~ambient_occlusion_pass() override = default;

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Ambient Occlusion";
  }

  auto declare(compute_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context) -> void override;

private:

  memory::observer_ptr<graphics::compute_pipeline> _occlusion_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _blur_pipeline{};

}; // class ambient_occlusion_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PASSES_AMBIENT_OCCLUSION_PASS_HPP_
