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
 * @brief Mirrors the master preview shader's `push_data`; must stay byte-identical. Has no frame_data, just one fixed light and one material.
 *
 * Exactly 128 bytes so the bindless pipeline layout can be reused; sampler_index is packed into light_direction.w as a float.
 */
struct shader_graph_preview_push_constants {
  sbx::math::matrix4x4 model_view_projection;
  sbx::math::vector4 light_direction; // xyz = direction (surface <- light), w = sampler index
  sbx::math::vector4 light_color;     // rgb = color, a = intensity
  sbx::math::vector4 camera_position; // xyz used; w = elapsed seconds for the Time node
  sbx::graphics::buffer::address_type vertex_address;
  sbx::graphics::buffer::address_type material_address;
}; // struct shader_graph_preview_push_constants

static_assert(sizeof(shader_graph_preview_push_constants) == 128u, "Preview push constants must exactly fill the engine's shared 128-byte push-constant budget.");

/**
 * @brief Mirrors frame_data.slang's `material_data`, byte-identical; one entry per open preview, rewritten from the editor's live values.
 *
 * Only the generic slots and the UV transform matter to a shader graph; the rest stays zero, with uv_tiling defaulting to (1, 1).
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

/** @brief Mirrors the node preview shader's `push_data`; must stay byte-identical. Just the shared material buffer and a sampler index. */
struct shader_graph_node_preview_push_constants {
  sbx::graphics::buffer::address_type material_address;
  std::float_t sampler_index; // a small index stored as float
  std::float_t time;
  std::float_t delta_time;
}; // struct shader_graph_node_preview_push_constants

} // namespace editor

#endif // EDITOR_PANELS_SHADER_GRAPH_PREVIEW_HPP_
