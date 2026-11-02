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

/**
 * @brief Depth-only pre-pass for the opaque list into the shared MSAA depth target, resolved into a single-sample scene_depth for the Scene Depth node.
 *
 * With occlusion culling on, a second (late) instance adds the late camera view on top, after hiz_pass built the pyramid from the first.
 */
class depth_pre_pass final : public graphics_pass {

public:

  /**
   * @brief Creates the pre-pass for the camera view, or for the late camera view.
   *
   * @param samples The depth target's sample count.
   * @param late Whether this instance draws the late camera view, which only runs with occlusion culling on.
   */
  explicit depth_pre_pass(graphics::samples samples, bool late = false);

  [[nodiscard]] auto name() const -> std::string_view override {
    return _late ? "Depth Pre Late" : "Depth Pre";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context, [[maybe_unused]] std::uint32_t group) const -> bool override {
    return !_late || context.occlusion_enabled;
  }

private:

  // Uses depth_vertex_main/depth_fragment_main, always generated so displaced geometry matches its color pass.
  auto _copy_scene_depth(render_context& context) -> void;

  [[nodiscard]] auto _resolve_custom_pipeline(const material_shader_source& source, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline>;

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u> _pipelines{};

  // min when the device supports it for depth resolves, otherwise sample_zero (the only guaranteed mode).
  graphics::resolve_mode _scene_depth_resolve_mode{graphics::resolve_mode::sample_zero};

  graphics::samples _samples;

  bool _late;

}; // class depth_pre_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_DEPTH_PRE_PASS_HPP_
