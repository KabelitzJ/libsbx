// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SHADOW_PASS_HPP_
#define LIBSBX_RENDER_SHADOW_PASS_HPP_

#include <array>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/assets/shader_graph.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Renders the sun's cascaded shadow maps: depth-only, alpha-cutout-aware (same
 * material-driven clip as depth_pre_pass), one cascade at a time into its own
 * shadow_map_resolution² target. Draws nothing when render_context::has_shadow_caster is false (no
 * shadow-casting directional light this frame — see scene_renderer_module::_build_packet), but its
 * groups still clear the maps then, since opaque_pass declares a read of them (see
 * render_graph's compiled_group::clear_on_skip).
 *
 * Runs after light_culling_pass and before opaque_pass, which samples the resulting maps (see
 * shaders/shadows/csm.slang) while shading the sun's contribution; opaque_pass's declared read is
 * what transitions them to shader_read_only. transparent_accumulate_pass's alpha-blend shading
 * policy doesn't receive shadows.
 */
class shadow_pass final : public graphics_pass {

public:

  shadow_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Shadow";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t cascade) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context, std::uint32_t cascade) const -> bool override;

private:

  // Same entry points as depth_pre_pass::_resolve_custom_pipeline, with the shadow cascade's own
  // cull/sample state, delegated to render::resolve_custom_pipeline.
  [[nodiscard]] auto _resolve_custom_pipeline(const std::string& shader_path, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline>;

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u> _pipelines{};

}; // class shadow_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_SHADOW_PASS_HPP_
