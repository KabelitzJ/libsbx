// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_MATERIAL_HPP_
#define LIBSBX_MODELS_MATERIAL_HPP_

#include <cinttypes>
#include <cmath>

#include <libsbx/utility/hash.hpp>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/images/image2d.hpp>

#include <libsbx/scenes/components/static_mesh.hpp>

#include <libsbx/models/vertex_stream.hpp>

namespace sbx::models {

enum class alpha_mode : std::uint8_t { 
  opaque, 
  mask,
  blend
}; // enum class alpha_mode

enum class material_feature : std::uint8_t {
  none = 0,
  cast_shadow = utility::bit_v<0>,
  receive_shadow = utility::bit_v<1>,
  invert_backface_normals = utility::bit_v<2>
}; // enum class material_feature

inline constexpr auto operator|(const material_feature lhs, const material_feature rhs) -> material_feature {
  return static_cast<material_feature>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

struct alignas(16) material_data {
  std::uint32_t albedo_image_index;
  std::uint32_t albedo_sampler_index;
  std::uint32_t normal_image_index;
  std::uint32_t normal_sampler_index;

  std::uint32_t metallic_roughness_image_index;
  std::uint32_t metallic_roughness_sampler_index;
  std::uint32_t occlusion_image_index;
  std::uint32_t occlusion_sampler_index;

  std::uint32_t emissive_image_index;
  std::uint32_t emissive_sampler_index;
  std::uint32_t height_image_index;
  std::uint32_t height_sampler_index;

  std::float_t height_scale;
  std::float_t height_offset;
  std::float_t parallax_min_layers;
  std::float_t parallax_max_layers;

  std::float_t emissive_strength;
  std::float_t normal_scale;
  std::float_t metallic_factor;
  std::float_t roughness_factor;

  std::float_t occlusion_strength;
  std::float_t specular_factor;
  std::float_t alpha_cutoff;
  std::uint32_t flags;

  math::vector2 uv_offset;
  math::vector2 uv_scale;

  math::color base_color;

  math::vector4 emissive_factor;

  std::float_t sway_speed_strength;
  std::float_t scrumble_speed_strength;
  std::float_t falloff_exponents;
  std::uint32_t _pad0;
}; // struct material_data

static_assert(sizeof(material_data) <= 256u);
static_assert(alignof(material_data) == 16u);

struct alignas(std::uint32_t) material_key {
  std::uint32_t surface_shader_hash;
  std::uint16_t alpha           : 2;
  std::uint16_t is_double_sided : 1;
  std::uint16_t stream_mask     : 5;
  std::uint16_t feature_mask    : 8;

  material_key() {
    std::memset(this, 0, sizeof(material_key));
  }

}; // struct material_key

static_assert(sizeof(material_key) == 8u);
static_assert(alignof(material_key) == alignof(std::uint32_t));

inline auto operator==(const material_key& lhs, const material_key& rhs) -> bool { 
  return std::memcmp(&lhs, &rhs, sizeof(material_key)) == 0; 
}

struct material_key_hash {
  auto operator()(const material_key& key) const noexcept -> std::size_t {
    return utility::djb2_hash{}({reinterpret_cast<const std::uint8_t*>(&key), sizeof(material_key)});
  }
}; // struct material_key_hash

struct texture_slot {
  graphics::image2d_handle image{};

  graphics::address_mode address_mode_u{graphics::address_mode::repeat};
  graphics::address_mode address_mode_v{graphics::address_mode::repeat};

  graphics::filter mag_filter{graphics::filter::linear};
  graphics::filter min_filter{graphics::filter::linear};
  std::float_t anisotropy{1.0f};
}; // struct texture_slot

struct texture_slot_hash {
  auto operator()(const texture_slot& texture_slot) const noexcept -> std::size_t;
}; // struct texture_slot_hash

struct material {

  math::color base_color{math::color::white()};

  std::float_t metallic_factor{1.0f};
  std::float_t roughness_factor{1.0f};
  std::float_t occlusion_strength{1.0f};
  std::float_t specular_factor{1.0f};

  math::vector4 emissive_factor{0, 0, 0, 1};
  std::float_t emissive_strength{1.0f};

  alpha_mode alpha{alpha_mode::opaque};
  std::float_t alpha_cutoff{0.9f};
  bool is_double_sided{false};

  texture_slot albedo{};
  texture_slot normal{};
  texture_slot metallic_roughness{};
  texture_slot occlusion{};
  texture_slot emissive{};
  texture_slot height{};

  std::float_t normal_scale{1.0f};

  std::float_t height_offset{0.0f};
  std::float_t height_scale{0.05f};

  std::float_t parallax_min_layers{8.0f};
  std::float_t parallax_max_layers{32.0f};

  std::float_t sway_speed{0.0f};
  std::float_t sway_strength{0.0f};
  std::float_t sway_falloff_exponent{3.0f};

  std::float_t scrumble_speed{0.0f};
  std::float_t scrumble_strength{0.0f};
  std::float_t scrumble_falloff_exponent{2.0f};

  math::vector2 uv_scale{1.0f, 1.0f};
  math::vector2 uv_offset{0.0f, 0.0f};

  utility::bit_field<material_feature> features{material_feature::cast_shadow | material_feature::receive_shadow | material_feature::invert_backface_normals};

  std::filesystem::path surface_shader{};
  utility::bit_field<vertex_stream> required_streams{};

  operator material_key() const {
    auto key = material_key{};

    key.alpha = static_cast<std::uint64_t>(alpha);
    key.is_double_sided = is_double_sided;
    key.stream_mask = required_streams.underlying();
    key.feature_mask = features.underlying();
    
    auto shader_path = surface_shader.generic_string();
    key.surface_shader_hash = utility::crc32(std::span{reinterpret_cast<const std::uint8_t*>(shader_path.data()), shader_path.size()});

    return key;
  }

}; // struct material
  
} // namespace sbx::models

template<>
struct std::hash<sbx::models::material_key> {
  auto operator()(const sbx::models::material_key& key) const noexcept -> std::size_t {
    return sbx::models::material_key_hash{}(key);
  }
}; // struct std::hash

#endif // LIBSBX_MODELS_MATERIAL_HPP_
