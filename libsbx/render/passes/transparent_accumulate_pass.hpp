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
 * @brief Weighted blended OIT accumulation (McGuire & Bavoil) of the transparent commands into an accumulator and a revealage target, each resolved for transparent_resolve_pass.
 *
 * Depth is tested, not written. The blend is order-independent, so no sorting is needed and double-sided objects draw once.
 * Pipeline slots: [0] pbr/back-cull, [1] pbr/double-sided, [2] unlit/back-cull, [3] unlit/double-sided.
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

  auto _resolve_custom_pipeline(const std::string& shader_path, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline>;

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u> _pipelines{};

}; // class transparent_accumulate_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_TRANSPARENT_ACCUMULATE_PASS_HPP_
