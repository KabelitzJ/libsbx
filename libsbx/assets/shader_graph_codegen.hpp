// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_SHADER_GRAPH_CODEGEN_HPP_
#define LIBSBX_ASSETS_SHADER_GRAPH_CODEGEN_HPP_

#include <cstdint>
#include <expected>
#include <string>

#include <libsbx/assets/shader_graph.hpp>

namespace sbx::assets {

/**
 * @brief Translates a shader_graph into a self-contained Slang file: an optional Vertex block for `vertex_main`/`depth_vertex_main` and a Lit or Unlit Fragment block for `fragment_main<Policy>`.
 *
 * Pure string generation, testable without a shader compiler.
 *
 * @param graph_name Part of the generated names; must already be a valid Slang identifier fragment.
 * @param graph The graph to translate.
 *
 * @return The generated source, or a readable reason it couldn't be generated.
 */
[[nodiscard]] auto generate_shader_graph_source(const std::string& graph_name, const shader_graph::create_info& graph) -> std::expected<std::string, std::string>;

/**
 * @brief Generates the graph editor's master preview shader: one sphere lit by one fixed directional light.
 *
 * Doesn't include geometry_common.slang or lighting.slang, which need a full frame_data; only frame_data.slang's struct shapes are reused.
 * Its `push_data`/`material_data` must stay byte-identical to editor/panels/shader_graph_preview.hpp.
 *
 * ponytail: shading is flat ambient plus Lambert on Albedo/Normal/Emission only; add a small Blinn-Phong term if the preview needs to show Metallic/Roughness.
 *
 * @param graph_name Part of the generated names; must already be a valid Slang identifier fragment.
 * @param graph The graph to translate.
 *
 * @return The generated source, or a readable reason it couldn't be generated.
 */
[[nodiscard]] auto generate_shader_graph_preview_source(const std::string& graph_name, const shader_graph::create_info& graph) -> std::expected<std::string, std::string>;

/**
 * @brief Generates a fullscreen 2D preview of one node's output pin for the graph editor's node swatches.
 *
 * Always evaluated in the fragment stage, so vertex-only inputs return an error.
 * Scalars show as grayscale, float2 as (x,y,0,1), float3 as (x,y,z,1), float4 as-is. Exposed values come from the live preview material buffer.
 *
 * @param graph_name Part of the generated names; must already be a valid Slang identifier fragment.
 * @param graph The graph containing the node.
 * @param node_id The node to preview.
 * @param output_pin The output pin to preview.
 *
 * @return The generated source, or a readable reason it couldn't be generated.
 */
[[nodiscard]] auto generate_node_preview_source(const std::string& graph_name, const shader_graph::create_info& graph, std::uint32_t node_id, std::uint32_t output_pin) -> std::expected<std::string, std::string>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SHADER_GRAPH_CODEGEN_HPP_
