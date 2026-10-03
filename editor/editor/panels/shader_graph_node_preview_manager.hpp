// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_SHADER_GRAPH_NODE_PREVIEW_MANAGER_HPP_
#define EDITOR_PANELS_SHADER_GRAPH_NODE_PREVIEW_MANAGER_HPP_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <imgui.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/async_shader_compiler.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/assets/shader_graph.hpp>

namespace editor {

/**
 * @brief Renders the per-node 2D preview swatches, each evaluating one node's output pin.
 *
 * Has its own async_shader_compiler: both previews drain their compiler's results, so sharing one would let either steal the other's results.
 * Reads the master preview's live material buffer instead of keeping a copy.
 */
class shader_graph_node_preview_manager final : public sbx::utility::noncopyable {

public:

  shader_graph_node_preview_manager() = default;

  ~shader_graph_node_preview_manager();

  /**
   * @brief Queues a node preview recompile, on toggling it on and on every structural edit. Coalesced per node.
   *
   * @param graph The graph's current contents.
   * @param node_id The previewed node.
   */
  auto request_recompile(const sbx::assets::shader_graph::create_info& graph, std::uint32_t node_id) -> void;

  /**
   * @brief Releases a node's preview when it's toggled off or deleted; no-op if it was never previewed.
   *
   * @param node_id The node.
   */
  auto forget(std::uint32_t node_id) -> void;

  /** @brief Releases every preview, e.g. when switching graphs, since node ids are graph-local. */
  auto clear() -> void;

  /**
   * @brief Picks up finished compiles and redraws only previews that changed.
   *
   * @param material_address The master preview's material buffer.
   * @param sampler_index The sampler's bindless index.
   * @param material_changed Whether the material values changed this frame.
   */
  auto update(sbx::graphics::buffer::address_type material_address, std::uint32_t sampler_index, bool material_changed) -> void;

  [[nodiscard]] auto texture_id(std::uint32_t node_id) const -> std::optional<ImTextureID>;

  [[nodiscard]] auto error(std::uint32_t node_id) const -> std::string;

private:

  struct entry {
    sbx::graphics::image_handle target{};
    std::unique_ptr<sbx::graphics::shader> shader{};
    std::unique_ptr<sbx::graphics::graphics_pipeline> pipeline{};
    std::string error{};
    bool resources_ready{false};
    bool has_rendered_once{false};
  }; // struct entry

  auto _ensure_target(entry& state, std::uint32_t node_id) -> void;

  auto _render(entry& state, sbx::graphics::buffer::address_type material_address, std::uint32_t sampler_index) -> void;

  static constexpr auto _extent = std::uint32_t{64u};

  sbx::graphics::async_shader_compiler _compiler{};
  std::unordered_map<std::uint32_t, entry> _entries{};

}; // class shader_graph_node_preview_manager

} // namespace editor

#endif // EDITOR_PANELS_SHADER_GRAPH_NODE_PREVIEW_MANAGER_HPP_
