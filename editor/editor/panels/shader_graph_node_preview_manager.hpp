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
 * @brief Renders the small per-node 2D preview swatches (shader_graph_node::preview) shown inline
 * on opted-in nodes -- one flat, lighting-free quad per previewed node, evaluating just that
 * node's own output pin (generate_node_preview_source). Owns its own async_shader_compiler,
 * separate from shader_graph_preview_renderer's (the master 3D preview) -- both classes' own
 * update() drains whatever compiler it owns via take_results(), so sharing one between two
 * independent consumers would mean either one could silently steal the other's result off the
 * queue; two small idle-most-of-the-time worker threads is a simpler, safer trade than teaching
 * async_shader_compiler to filter per-consumer.
 *
 * Reads the master preview's own material buffer address/sampler index (passed into update() each
 * call, not owned here) rather than maintaining a second copy -- an exposed constant/texture_sample
 * node previews the same live graph state the master preview already keeps that buffer in sync with.
 */
class shader_graph_node_preview_manager final : public sbx::utility::noncopyable {

public:

  shader_graph_node_preview_manager() = default;

  ~shader_graph_node_preview_manager();

  /** @brief Call once when a node's preview is freshly toggled on, and once per structural edit for every currently-previewed node (a structural change anywhere could affect any previewed node's own subgraph). Coalesced per node_id, like shader_graph_preview_renderer's own request_recompile. */
  auto request_recompile(const sbx::assets::shader_graph::create_info& graph, std::uint32_t node_id) -> void;

  /** @brief Releases @p node_id's preview resources -- call when its preview is toggled off or the node itself is deleted. A no-op if it was never previewed. */
  auto forget(std::uint32_t node_id) -> void;

  /** @brief Releases every tracked node's preview resources -- call when switching to a different shader graph (node ids are graph-local, so a stale entry from the previous graph would otherwise just sit there orphaned, never forgotten by that graph's own deletions). */
  auto clear() -> void;

  /**
   * @brief Drains ready compiles and redraws whichever tracked nodes actually changed (a new
   * pipeline just became ready, or @p material_changed) -- see shader_graph_preview_renderer's own
   * update() doc comment for why a redraw-only-when-changed policy matters here too.
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
