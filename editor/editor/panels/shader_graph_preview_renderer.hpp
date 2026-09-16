// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_SHADER_GRAPH_PREVIEW_RENDERER_HPP_
#define EDITOR_PANELS_SHADER_GRAPH_PREVIEW_RENDERER_HPP_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <imgui.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/async_shader_compiler.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/assets/shader_graph.hpp>
#include <libsbx/assets/mesh.hpp>

#include <editor/panels/shader_graph_preview.hpp>

namespace editor {

/**
 * @brief Renders the shader graph editor's always-visible master preview: one sphere, one fixed
 * directional light, shaded by the graph's own current Fragment output via
 * generate_shader_graph_preview_source (shader_graph_codegen.hpp). Owns its own
 * async_shader_compiler (see its own doc comment for why previews can't share the render thread's
 * shader_cache) -- structural edits enqueue a recompile there; value-only edits (dragging an
 * exposed constant, picking a texture) just rewrite this preview's own one-entry material buffer
 * and redraw, no recompile needed.
 *
 * Renders via a blocking one-off command buffer (graphics::command_buffer::submit_idle()) rather
 * than participating in the main frame's pipelining -- fine since it only actually redraws when
 * something changed (see update()'s own doc comment), not every frame.
 */
class shader_graph_preview_renderer final : public sbx::utility::noncopyable {

public:

  shader_graph_preview_renderer();

  ~shader_graph_preview_renderer();

  /**
   * @brief Call on every structural edit (see shader_graph_panel's own doc comment for the
   * structural-vs-value distinction) -- generates and asynchronously submits a recompile for @p
   * graph's current shape. Coalesced by async_shader_compiler: a call before the previous one
   * finishes compiling simply replaces it, so a burst of edits only ever compiles the latest one.
   */
  auto request_recompile(const sbx::assets::shader_graph::create_info& graph) -> void;

  /**
   * @brief Refreshes the preview's live material values from @p graph (cheap; no recompile),
   * drains any background compile result that's ready, and redraws only when something actually
   * changed (a new pipeline just became ready, or a material value did) -- a continuous per-frame
   * redraw would mean a blocking GPU round-trip every single frame the panel is open, for a
   * preview that has no reason to change most of those frames.
   *
   * @return This frame's preview image as an ImGui texture id, or nullopt if nothing has compiled
   * successfully yet (graph doesn't compile, or hasn't finished its first compile).
   */
  [[nodiscard]] auto update(const sbx::assets::shader_graph::create_info& graph) -> std::optional<ImTextureID>;

  /** @brief Whether the material values this update() call refreshed actually differed from last call's -- shader_graph_node_preview_manager's own update() uses this to know when it needs to redraw too, since its previewed nodes read the same buffer. */
  [[nodiscard]] auto material_changed_last_update() const noexcept -> bool {
    return _material_changed_last_update;
  }

  [[nodiscard]] auto error() const noexcept -> const std::string& {
    return _error;
  }

  // Both let shader_graph_node_preview_manager's own per-node previews read the exact same live
  // material values/sampler this preview already keeps refreshed, rather than maintaining a
  // second copy -- see that class's own doc comment. Zero/default until _ensure_resources has run
  // at least once (i.e. before this renderer's own first update() call).
  [[nodiscard]] auto material_address() const noexcept -> sbx::graphics::buffer::address_type {
    return _material_address;
  }

  [[nodiscard]] auto sampler_index() const noexcept -> std::uint32_t {
    return _sampler_index;
  }

private:

  auto _ensure_resources() -> void;

  [[nodiscard]] auto _refresh_material(const sbx::assets::shader_graph::create_info& graph) -> bool;

  auto _render() -> void;

  static constexpr auto _key = sbx::graphics::async_shader_compiler::key_type{0u};
  static constexpr auto _extent = std::uint32_t{200u};

  sbx::graphics::async_shader_compiler _compiler{};

  sbx::assets::mesh_handle _sphere{};
  std::uint32_t _sampler_index{0u};

  bool _resources_ready{false};
  sbx::graphics::image_handle _target{};
  sbx::graphics::buffer_handle _material_buffer{};
  sbx::graphics::buffer::address_type _material_address{0u};

  std::unique_ptr<sbx::graphics::shader> _shader{};
  std::unique_ptr<sbx::graphics::graphics_pipeline> _pipeline{};

  shader_graph_preview_material_data _last_material{};
  bool _has_material{false}; // false until _refresh_material's first call -- forces the first redraw's material write

  std::string _error{};
  bool _has_rendered_once{false};
  bool _material_changed_last_update{false};

}; // class shader_graph_preview_renderer

} // namespace editor

#endif // EDITOR_PANELS_SHADER_GRAPH_PREVIEW_RENDERER_HPP_
