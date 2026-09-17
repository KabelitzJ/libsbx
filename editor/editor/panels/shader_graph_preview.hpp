// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_SHADER_GRAPH_PREVIEW_HPP_
#define EDITOR_PANELS_SHADER_GRAPH_PREVIEW_HPP_

#include <array>
#include <cstdint>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/assets/shader_graph.hpp>

namespace editor {

/**
 * @brief C++ mirror of the `push_data` struct generate_shader_graph_preview_source
 * (shader_graph_codegen.cpp) emits into every master-preview shader -- must stay byte-identical
 * to it. Deliberately its own small layout, not the real render_pass.hpp push_constants/
 * geometry_common.slang push_data: a preview has no frame_data (no light list, shadow cascades,
 * or cluster-light buffer to populate), just this one fixed light and one throwaway material.
 *
 * Exactly 128 bytes -- the same push-constant budget every other pipeline in the engine already
 * uses (see render_pass.hpp's own push_constants and its static_assert), so the preview pipeline
 * can reuse sbx::graphics::bindless_table::pipeline_layout() unchanged instead of needing a layout of
 * its own. sampler_index is packed into light_direction's otherwise-unused .w (as a plain float,
 * always a small non-negative index -- safely round-trips through float32) rather than spending a
 * whole field on it, to fit the budget alongside the material/vertex buffer addresses.
 */
struct shader_graph_preview_push_constants {
  sbx::math::matrix4x4 model_view_projection;
  sbx::math::vector4 light_direction; // xyz = direction (surface <- light), w = float(sampler_index)
  sbx::math::vector4 light_color;     // rgb = color, a = intensity
  sbx::math::vector4 camera_position; // xyz used; w = float(elapsed seconds) for the Time node -- Delta Time is a fixed literal in the generated shader instead, no field needed for it
  sbx::graphics::buffer::address_type vertex_address;
  sbx::graphics::buffer::address_type material_address;
}; // struct shader_graph_preview_push_constants

static_assert(sizeof(shader_graph_preview_push_constants) == 128u, "Preview push constants must exactly fill the engine's shared 128-byte push-constant budget.");

/**
 * @brief C++ mirror of frame_data.slang's `material_data`, byte-identical to it and to
 * asset_residency.cpp's own private copy (not reusable directly -- that one has no external
 * linkage) -- one throwaway entry per open shader-graph preview, rewritten every frame from the
 * editor's own live (possibly unsaved) node values, resolved via asset_residency the same way a
 * real material's generic_params/generic_textures are. Every field but generic_params/
 * generic_textures/uv_tiling/uv_offset is irrelevant to a shader_graph-shaded surface (same as the
 * real material's own shading_model doc comment already notes) and is left zeroed; uv_tiling
 * defaults to identity (1,1) rather than zero since apply_uv_transform multiplies by it.
 */
struct shader_graph_preview_material_data {
  sbx::math::vector4 base_color_factor{};
  sbx::math::vector4 emissive_factor{};
  std::uint32_t albedo_index{0u};
  std::uint32_t normal_index{0u};
  std::uint32_t metallic_roughness_index{0u};
  std::uint32_t occlusion_index{0u};
  std::uint32_t emissive_index{0u};
  std::float_t metallic_factor{0.0f};
  std::float_t roughness_factor{0.0f};
  std::float_t alpha_cutoff{0.0f};
  std::uint32_t flags{0u};
  std::float_t normal_scale{0.0f};
  std::float_t occlusion_strength{0.0f};
  std::float_t emissive_strength{0.0f};
  std::float_t ior{0.0f};
  sbx::math::vector2 uv_tiling{1.0f, 1.0f};
  sbx::math::vector2 uv_offset{0.0f, 0.0f};
  std::array<sbx::math::vector4, sbx::assets::shader_graph_max_params> generic_params{};
  std::array<std::uint32_t, sbx::assets::shader_graph_max_textures> generic_textures{};

  auto operator==(const shader_graph_preview_material_data&) const -> bool = default;
}; // struct shader_graph_preview_material_data

/**
 * @brief C++ mirror of the `push_data` struct generate_node_preview_source (shader_graph_codegen.cpp)
 * emits into every per-node preview shader -- must stay byte-identical to it. Much smaller than the
 * master preview's own push_data: a flat 2D swatch has no vertex/light/camera to feed at all, just
 * the shared material buffer (see shader_graph_preview_material_data) and sampler index.
 */
struct shader_graph_node_preview_push_constants {
  sbx::graphics::buffer::address_type material_address;
  std::float_t sampler_index; // small non-negative index, stored as a plain float -- see sampler_index's own doc comment on shader_graph_preview_push_constants
  std::float_t time;
  std::float_t delta_time;
}; // struct shader_graph_node_preview_push_constants

} // namespace editor

#endif // EDITOR_PANELS_SHADER_GRAPH_PREVIEW_HPP_
