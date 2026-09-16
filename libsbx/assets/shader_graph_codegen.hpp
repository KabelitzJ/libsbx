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
 * @brief Translates a shader_graph into a self-contained Slang source file: its optional Vertex
 * block (Position/Normal/Tangent, defaulting to pass-through) feeding `vertex_main` and
 * `depth_vertex_main`, and its required Fragment block (Lit -- metallic-roughness surface
 * properties, modeled on Unity Shader Graph's PBR Master Node, feeding shaders/lighting.slang's
 * `evaluate_lit_surface`, or Unlit -- a flat color/alpha with no lighting at all; both also carry
 * an Alpha + Alpha Clip Threshold pair that `clip()`s the fragment when the material is alpha-
 * masked) feeding `fragment_main<Policy>`. See shader_graph_codegen.cpp's doc comment for the
 * generated file's exact shape. Pure function, no Vulkan/Slang-API dependency -- callable and
 * testable without a real shader compiler present.
 *
 * @param graph_name Becomes part of the generated type/entry-point names -- must already be a
 * valid Slang identifier fragment (the caller's job, e.g. derived from the asset's uuid).
 * @return The generated Slang source, or a human-readable reason it couldn't be generated.
 */
[[nodiscard]] auto generate_shader_graph_source(const std::string& graph_name, const shader_graph::create_info& graph) -> std::expected<std::string, std::string>;

/**
 * @brief Translates a shader_graph into a self-contained Slang source file for the graph editor's
 * master 3D preview (one sphere, one fixed directional light) -- deliberately NOT the same
 * shading as generate_shader_graph_source's real fragment_main<Policy>: no #include of
 * geometry_common.slang (its push_data would collide with this file's own, smaller one), no
 * frame_data/lighting.slang/evaluate_lit_surface (that needs a fully-populated frame_data --
 * light list, shadow cascades, a real cluster-light buffer lighting.slang unconditionally
 * dereferences -- none of which a one-light preview has any use for). Only
 * `#include <frame_data.slang>` for the `vertex`/`material_data` struct shapes and
 * `apply_uv_transform`, reused as-is.
 *
 * ponytail: the preview's own shading is a flat ambient term plus single-light Lambertian
 * diffuse on Albedo/Normal/Emission only -- Metallic/Roughness/Occlusion/Alpha are evaluated for
 * validation but not reflected in the preview's look. Revisit (a small Blinn-Phong specular term,
 * still without needing evaluate_lit_surface/frame_data) if that turns out to matter in practice.
 *
 * See editor/panels/shader_graph_preview.hpp for the push-constant/material-buffer layout this
 * Slang source's own `push_data`/`material_data` must stay byte-identical to.
 */
[[nodiscard]] auto generate_shader_graph_preview_source(const std::string& graph_name, const shader_graph::create_info& graph) -> std::expected<std::string, std::string>;

/**
 * @brief Translates one node's single output pin into a self-contained flat 2D preview shader
 * (a fullscreen triangle, no vertex buffer/mesh needed at all -- see the .cpp for the classic
 * SV_VertexID trick) for the graph editor's per-node preview swatch (shader_graph_node::preview).
 * Evaluated in the fragment stage regardless of where @p node_id actually sits in the graph, so a
 * node reachable only from the Vertex block (Input Vertex Position/Normal/Tangent) can't be
 * node-previewed -- there's no vertex context for a flat 2D swatch to evaluate it in; this returns
 * an error for that case rather than silently substituting something else.
 *
 * The result is converted to a display color by @p output_pin's own resolved width: scalar shows
 * as grayscale, vector2 as (x,y,0,1), vector3 as (x,y,z,1), vector4 as-is -- matches Unity Shader
 * Graph's own node-preview convention.
 *
 * Reuses `material_data`/apply_uv_transform from frame_data.slang exactly like
 * generate_shader_graph_preview_source, through the SAME material buffer that one already
 * maintains (see shader_graph_node_preview_renderer) -- an exposed constant/texture_sample node
 * previews the graph's current live value, not the real material's.
 */
[[nodiscard]] auto generate_node_preview_source(const std::string& graph_name, const shader_graph::create_info& graph, std::uint32_t node_id, std::uint32_t output_pin) -> std::expected<std::string, std::string>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SHADER_GRAPH_CODEGEN_HPP_
