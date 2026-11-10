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
 * @brief The built-in depth-only pipelines shadow passes draw casters with, indexed by draw_command::pipeline_id (bit 0 double-sided; shading model doesn't affect depth).
 *
 * @param name The pipelines' debug name.
 *
 * @return The pipelines.
 */
[[nodiscard]] auto create_shadow_caster_pipelines(std::string_view name) -> std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>;

/**
 * @brief A custom-shader material's shadow caster pipeline: its depth entry points with shadow state, or the built-in pipeline when it has none (like Unity's fallback shadow caster).
 *
 * @param pipelines The pass's built-in pipelines, from create_shadow_caster_pipelines.
 * @param source The material's shader.
 * @param is_double_sided Whether to skip back-face culling.
 * @param name The pipeline's debug name.
 *
 * @return The pipeline, or null if the shader has no usable one.
 */
[[nodiscard]] auto resolve_shadow_caster_pipeline(const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>& pipelines, const material_shader_source& source, bool is_double_sided, std::string_view name) -> memory::observer_ptr<graphics::graphics_pipeline>;

/**
 * @brief Renders the sun's cascaded shadow maps depth-only, one cascade at a time, with the same alpha cutout as depth_pre_pass.
 *
 * Draws nothing without a shadow-casting light but still clears the maps, since opaque_pass declares a read of them. Runs after light_culling_pass and before opaque_pass.
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

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u> _pipelines{};

}; // class shadow_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_SHADOW_PASS_HPP_
