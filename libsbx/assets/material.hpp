// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_MATERIAL_HPP_
#define LIBSBX_ASSETS_MATERIAL_HPP_

#include <array>
#include <cstdint>
#include <limits>
#include <string>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>
#include <libsbx/assets/texture.hpp>
#include <libsbx/assets/shader_graph.hpp>

namespace sbx::assets {

enum class alpha_mode : std::uint8_t {
  opaque, // fully opaque
  mask,   // alpha-tested against alpha_cutoff (discard), still opaque pass
  blend   // order-dependent transparency, transparent pass
}; // enum class alpha_mode

// The engine's only two built-in shading models. Anything else (toon/cel, etc.) is composed with a
// shader graph material, never a built-in the engine ships -- see the shader graph roadmap.
enum class shading_model : std::uint8_t {
  pbr,
  unlit
}; // enum class shading_model

class material final : public loadable {

  friend class asset_residency;

public:

  inline static constexpr auto invalid_index = std::numeric_limits<std::uint32_t>::max();

  struct create_info {
    std::string name{"material"};
    math::color base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    math::vector3 emissive_factor{0.0f, 0.0f, 0.0f};
    std::float_t metallic_factor{1.0f};
    std::float_t roughness_factor{1.0f};
    assets::alpha_mode alpha{alpha_mode::opaque};
    assets::shading_model shading{shading_model::pbr};
    std::float_t alpha_cutoff{0.5f};
    bool is_double_sided{false};
    bool casts_shadow{true};
    bool receives_shadow{true};
    std::float_t normal_scale{1.0f};
    std::float_t occlusion_strength{1.0f};
    std::float_t emissive_strength{1.0f};
    std::float_t ior{1.5f}; // KHR_materials_ior default; F0 = ((ior-1)/(ior+1))^2 = 0.04
    math::vector2 uv_tiling{1.0f, 1.0f};
    math::vector2 uv_offset{0.0f, 0.0f};
    texture_handle albedo{};
    texture_handle normal{};
    texture_handle metallic_roughness{};
    texture_handle occlusion{};
    texture_handle emissive{};

    // Non-nil routes rendering to this graph's own generated shader instead of the built-in
    // pbr/unlit path (`shading` above becomes irrelevant once this is set). generic_params/
    // generic_textures hold this material's own values for that graph's exposed parameters --
    // see shader_graph::parameters() for their declared name/type/slot, in the same order these
    // arrays are indexed by (slot 0 first, etc.).
    shader_graph_handle shader_graph{};
    std::array<math::vector4, shader_graph_max_params> generic_params{};
    std::array<texture_handle, shader_graph_max_textures> generic_textures{};
  }; // struct create_info

  material() = default;

  explicit material(const create_info& create_info)
  : _base_color_factor{create_info.base_color_factor},
    _emissive_factor{create_info.emissive_factor},
    _metallic_factor{create_info.metallic_factor},
    _roughness_factor{create_info.roughness_factor},
    _alpha{create_info.alpha},
    _shading{create_info.shading},
    _alpha_cutoff{create_info.alpha_cutoff},
    _is_double_sided{create_info.is_double_sided},
    _casts_shadow{create_info.casts_shadow},
    _receives_shadow{create_info.receives_shadow},
    _normal_scale{create_info.normal_scale},
    _occlusion_strength{create_info.occlusion_strength},
    _emissive_strength{create_info.emissive_strength},
    _ior{create_info.ior},
    _uv_tiling{create_info.uv_tiling},
    _uv_offset{create_info.uv_offset},
    _albedo{create_info.albedo},
    _normal{create_info.normal},
    _metallic_roughness{create_info.metallic_roughness},
    _occlusion{create_info.occlusion},
    _emissive{create_info.emissive},
    _shader_graph{create_info.shader_graph},
    _generic_params{create_info.generic_params},
    _generic_textures{create_info.generic_textures},
    _name{create_info.name} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _index != invalid_index;
  }

  [[nodiscard]] auto index() const noexcept -> std::uint32_t {
    return _index;
  }

  [[nodiscard]] auto base_color_factor() const noexcept -> const math::color& {
    return _base_color_factor;
  }

  [[nodiscard]] auto emissive_factor() const noexcept -> const math::vector3& {
    return _emissive_factor;
  }

  [[nodiscard]] auto metallic_factor() const noexcept -> std::float_t {
    return _metallic_factor;
  }

  [[nodiscard]] auto roughness_factor() const noexcept -> std::float_t {
    return _roughness_factor;
  }

  [[nodiscard]] auto alpha() const noexcept -> alpha_mode {
    return _alpha;
  }

  [[nodiscard]] auto shading() const noexcept -> shading_model {
    return _shading;
  }

  [[nodiscard]] auto alpha_cutoff() const noexcept -> std::float_t {
    return _alpha_cutoff;
  }

  [[nodiscard]] auto is_double_sided() const noexcept -> bool {
    return _is_double_sided;
  }

  [[nodiscard]] auto casts_shadow() const noexcept -> bool {
    return _casts_shadow;
  }

  [[nodiscard]] auto receives_shadow() const noexcept -> bool {
    return _receives_shadow;
  }

  [[nodiscard]] auto normal_scale() const noexcept -> std::float_t {
    return _normal_scale;
  }

  [[nodiscard]] auto occlusion_strength() const noexcept -> std::float_t {
    return _occlusion_strength;
  }

  [[nodiscard]] auto emissive_strength() const noexcept -> std::float_t {
    return _emissive_strength;
  }

  [[nodiscard]] auto ior() const noexcept -> std::float_t {
    return _ior;
  }

  [[nodiscard]] auto uv_tiling() const noexcept -> const math::vector2& {
    return _uv_tiling;
  }

  [[nodiscard]] auto uv_offset() const noexcept -> const math::vector2& {
    return _uv_offset;
  }

  [[nodiscard]] auto albedo() const noexcept -> const texture_handle& {
    return _albedo;
  }

  [[nodiscard]] auto normal() const noexcept -> const texture_handle& {
    return _normal;
  }

  [[nodiscard]] auto metallic_roughness() const noexcept -> const texture_handle& {
    return _metallic_roughness;
  }

  [[nodiscard]] auto occlusion() const noexcept -> const texture_handle& {
    return _occlusion;
  }

  [[nodiscard]] auto emissive() const noexcept -> const texture_handle& {
    return _emissive;
  }

  [[nodiscard]] auto shader_graph() const noexcept -> const shader_graph_handle& {
    return _shader_graph;
  }

  [[nodiscard]] auto generic_params() const noexcept -> const std::array<math::vector4, shader_graph_max_params>& {
    return _generic_params;
  }

  [[nodiscard]] auto generic_textures() const noexcept -> const std::array<texture_handle, shader_graph_max_textures>& {
    return _generic_textures;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& {
    return _name;
  }

private:

  math::color _base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
  math::vector3 _emissive_factor{0.0f, 0.0f, 0.0f};
  std::float_t _metallic_factor{1.0f};
  std::float_t _roughness_factor{1.0f};
  alpha_mode _alpha{alpha_mode::opaque};
  shading_model _shading{shading_model::pbr};
  std::float_t _alpha_cutoff{0.5f};
  bool _is_double_sided{false};
  bool _casts_shadow{true};
  bool _receives_shadow{true};
  std::float_t _normal_scale{1.0f};
  std::float_t _occlusion_strength{1.0f};
  std::float_t _emissive_strength{1.0f};
  std::float_t _ior{1.5f};
  math::vector2 _uv_tiling{1.0f, 1.0f};
  math::vector2 _uv_offset{0.0f, 0.0f};
  texture_handle _albedo{};
  texture_handle _normal{};
  texture_handle _metallic_roughness{};
  texture_handle _occlusion{};
  texture_handle _emissive{};
  shader_graph_handle _shader_graph{};
  std::array<math::vector4, shader_graph_max_params> _generic_params{};
  std::array<texture_handle, shader_graph_max_textures> _generic_textures{};
  std::uint32_t _index{invalid_index};
  math::uuid _id{math::uuid::nil()};
  std::string _name{"material"};

}; // class material

using material_handle = asset_handle<material>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_MATERIAL_HPP_
