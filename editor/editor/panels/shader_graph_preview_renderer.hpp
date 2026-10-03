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
 * @brief Renders the shader graph editor's master preview: one sphere under one fixed light, shaded by the graph's Fragment output.
 *
 * Compiles on its own async_shader_compiler; structural edits recompile, value edits only rewrite its material buffer.
 * Draws with a blocking one-off command buffer, which is fine because it only redraws when something changed.
 */
class shader_graph_preview_renderer final : public sbx::utility::noncopyable {

public:

  shader_graph_preview_renderer();

  ~shader_graph_preview_renderer();

  /**
   * @brief Queues a recompile for a structural edit; a newer request replaces one still compiling.
   *
   * @param graph The graph's current contents.
   */
  auto request_recompile(const sbx::assets::shader_graph::create_info& graph) -> void;

  /**
   * @brief Refreshes the material values, picks up a finished compile, and redraws only when either changed.
   *
   * @param graph The graph's current contents.
   *
   * @return The preview as an ImGui texture, or nullopt until a compile succeeded.
   */
  [[nodiscard]] auto update(const sbx::assets::shader_graph::create_info& graph) -> std::optional<ImTextureID>;

  /**
   * @brief Whether the last update() changed the material values; node previews read the same buffer and redraw too.
   *
   * @return True if the values changed.
   */
  [[nodiscard]] auto material_changed_last_update() const noexcept -> bool {
    return _material_changed_last_update;
  }

  [[nodiscard]] auto error() const noexcept -> const std::string& {
    return _error;
  }

  // Shared with the node previews so they read the same live material values. Zero until the first update().
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
  bool _has_material{false}; // forces the first redraw to write the material

  std::string _error{};
  bool _has_rendered_once{false};
  bool _material_changed_last_update{false};

}; // class shader_graph_preview_renderer

} // namespace editor

#endif // EDITOR_PANELS_SHADER_GRAPH_PREVIEW_RENDERER_HPP_
