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

  // Same entry points as depth_pre_pass, with the cascade's cull and sample state.
  [[nodiscard]] auto _resolve_custom_pipeline(const std::string& shader_path, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline>;

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u> _pipelines{};

}; // class shadow_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_SHADOW_PASS_HPP_
