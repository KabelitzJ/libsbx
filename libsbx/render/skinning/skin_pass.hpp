// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SKINNING_SKIN_PASS_HPP_
#define LIBSBX_RENDER_SKINNING_SKIN_PASS_HPP_

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Skins every visible skinned-mesh instance once per frame into scene_renderer_module's
 * shared scratch vertex buffer, before depth_pre_pass/shadow_pass/opaque_pass read it via the
 * ordinary vertex_address mechanism (draw_command::vertex_address_override) -- so a skinned
 * character is skinned exactly once per frame regardless of how many passes/cascades later draw
 * it, instead of re-skinning per pass.
 */
class skin_pass final : public compute_pass {

public:

  skin_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Skin";
  }

  auto declare(compute_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context) -> void override;

private:

  memory::observer_ptr<graphics::compute_pipeline> _pipeline{};

}; // class skin_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_SKINNING_SKIN_PASS_HPP_
