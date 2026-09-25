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
 * @brief Depth-only pre-pass: clears + writes the shared (4x MSAA) depth target from the opaque
 * list, and resolves it into resources.scene_depth as a byproduct of that same draw -- a plain,
 * single-sample, bindless-sampleable whole-scene depth the Scene Depth shader graph node reads
 * (scene_renderer_module.cpp owns the actual image/bindless index; see its own doc comment). Chosen
 * over a second depth-only draw specifically to avoid paying for the opaque silhouette twice.
 */
class depth_pre_pass final : public graphics_pass {

public:

  depth_pre_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Depth Pre";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

private:

  // This pass's own entry points (depth_vertex_main/depth_fragment_main -- always present in a
  // generated graph file regardless of whether it has a Vertex block, see shader_graph_codegen.cpp,
  // so a vertex-displacing graph gets a depth pre-pass that actually matches its own color pass) and
  // pipeline state for render::resolve_custom_pipeline.
  [[nodiscard]] auto _resolve_custom_pipeline(const std::string& shader_path, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline>;

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u> _pipelines{};

  // Queried once from the physical device at construction (see depth_pre_pass.cpp) -- only
  // sample_zero is spec-guaranteed for a DEPTH resolve, so this is min when the device actually
  // supports it, sample_zero otherwise. Never re-queried; a device's supported resolve modes don't
  // change at runtime.
  graphics::resolve_mode _scene_depth_resolve_mode{graphics::resolve_mode::sample_zero};

}; // class depth_pre_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_DEPTH_PRE_PASS_HPP_
