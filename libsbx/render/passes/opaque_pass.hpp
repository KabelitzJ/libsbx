// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_OPAQUE_PASS_HPP_
#define LIBSBX_RENDER_OPAQUE_PASS_HPP_

#include <array>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Opaque forward color pass: transitions the MSAA color target from undefined and owns
 * the depth "first reader" barrier (depth_pre_pass wrote it; this is the first pass to read it
 * back). CLEARs color, LOADs depth, draws context.packet->opaque_commands.
 */
class opaque_pass final : public graphics_pass {

public:

  opaque_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Opaque";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

private:

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _pipelines{};

}; // class opaque_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_OPAQUE_PASS_HPP_
