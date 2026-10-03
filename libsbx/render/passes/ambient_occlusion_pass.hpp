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
 * @brief Half-resolution screen-space ambient occlusion from the resolved scene depth: a rotated, range-checked hemisphere of samples, then a depth-aware blur. Lighting multiplies the ambient term by it.
 *
 * Like bloom_pass it never skips: with AO off it still transitions both targets to read-only for opaque_pass's declared read, and the frame's AO index is 0xFFFFFFFF.
 * Runs after depth_pre_pass, before opaque_pass.
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
