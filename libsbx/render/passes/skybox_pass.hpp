// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SKYBOX_PASS_HPP_
#define LIBSBX_RENDER_SKYBOX_PASS_HPP_

#include <array>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/** @brief Draws the skybox as a far-plane fullscreen triangle. Runs before transparents, which don't write depth and would otherwise be overwritten. */
class skybox_pass final : public graphics_pass {

public:

  skybox_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Skybox";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

}; // class skybox_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_SKYBOX_PASS_HPP_
