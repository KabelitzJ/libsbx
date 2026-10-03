// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_DEPTH_PRE_PASS_HPP_
#define LIBSBX_RENDER_DEPTH_PRE_PASS_HPP_

#include <array>
#include <string>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/assets/shader_graph.hpp>

#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/** @brief Depth-only pre-pass for the opaque list into the shared MSAA depth target, resolved into a single-sample scene_depth for the Scene Depth node. */
class depth_pre_pass final : public graphics_pass {

public:

  depth_pre_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Depth Pre";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

private:

  // Uses depth_vertex_main/depth_fragment_main, always generated so displaced geometry matches its color pass.
  [[nodiscard]] auto _resolve_custom_pipeline(const std::string& shader_path, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline>;

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u> _pipelines{};

  // min when the device supports it for depth resolves, otherwise sample_zero (the only guaranteed mode).
  graphics::resolve_mode _scene_depth_resolve_mode{graphics::resolve_mode::sample_zero};

}; // class depth_pre_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_DEPTH_PRE_PASS_HPP_
