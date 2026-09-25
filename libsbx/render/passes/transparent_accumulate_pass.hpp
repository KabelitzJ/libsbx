// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_TRANSPARENT_ACCUMULATE_PASS_HPP_
#define LIBSBX_RENDER_TRANSPARENT_ACCUMULATE_PASS_HPP_

#include <array>
#include <string>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>

#include <libsbx/assets/shader_graph.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Weighted Blended OIT (McGuire & Bavoil) accumulation: draws context.packet->transparent_commands
 * into two fresh MSAA targets — an accumulator (weighted premultiplied color+alpha, additive blend)
 * and a revealage buffer (product of (1 - alpha), multiplicative blend) — each resolved into a
 * single-sample target consumed by transparent_resolve_pass. Depth is LOADed (test only, no write)
 * against the depth buffer opaque_pass/depth_pre_pass wrote; no depth barrier needed, same reasoning
 * skybox_pass documents (depth never left depth_attachment_optimal since opaque_pass).
 *
 * Unlike naive back-to-front blending, WBOIT's blend equations are commutative — draw order doesn't
 * affect the result — so context.packet->transparent_commands needs no sorting, and a double-sided
 * object needs only one draw (pipeline 1, cull_mode::none) instead of a sorted front/back pair.
 *
 * Pipeline slots: [0] pbr/back-cull. [1] pbr/double-sided. [2] unlit/back-cull. [3] unlit/double-sided.
 */
class transparent_accumulate_pass final : public graphics_pass {

public:

  transparent_accumulate_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Transparent Accumulate";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

private:

  auto _make_pipeline(memory::observer_ptr<const graphics::shader> shader, graphics::cull_mode cull, const std::string& name) -> memory::observer_ptr<graphics::graphics_pipeline>;

  /** @brief Same as opaque_pass's -- see its doc comment. */
  auto _resolve_custom_pipeline(const std::string& shader_path, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline>;

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u> _pipelines{};

}; // class transparent_accumulate_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_TRANSPARENT_ACCUMULATE_PASS_HPP_
