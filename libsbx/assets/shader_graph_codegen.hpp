// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_SHADER_GRAPH_CODEGEN_HPP_
#define LIBSBX_ASSETS_SHADER_GRAPH_CODEGEN_HPP_

#include <string>

#include <libsbx/assets/shader_graph.hpp>

namespace sbx::assets {

struct shader_graph_codegen_result {
  bool success{false};
  std::string source{}; // generated Slang source -- valid only if success
  std::string error{};  // human-readable reason -- valid only if !success
}; // struct shader_graph_codegen_result

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
 */
[[nodiscard]] auto generate_shader_graph_source(const std::string& graph_name, const shader_graph::create_info& graph) -> shader_graph_codegen_result;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SHADER_GRAPH_CODEGEN_HPP_
