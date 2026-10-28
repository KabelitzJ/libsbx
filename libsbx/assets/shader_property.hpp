// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_SHADER_PROPERTY_HPP_
#define LIBSBX_ASSETS_SHADER_PROPERTY_HPP_

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/assets/texture2d.hpp>

namespace sbx::assets {

inline constexpr auto material_max_params = std::uint32_t{8u};
inline constexpr auto material_max_textures = std::uint32_t{16u};

// generic_params seen as one array of floats; value properties are packed into it by component.
inline constexpr auto material_max_param_components = material_max_params * 4u;

using shader_property_value = std::variant<std::monostate, std::float_t, math::vector2, math::vector3, math::vector4, math::color, texture2d_handle, std::string>;

inline constexpr auto shader_property_invalid_id = std::uint32_t{0u};

enum class shader_property_type : std::uint8_t {
  float_value,
  vector2_value,
  vector3_value,
  vector4_value,
  color_value,
  texture_value
}; // enum class shader_property_type

[[nodiscard]] inline auto shader_property_type_to_string(shader_property_type type) -> const char* {
  switch (type) {
    case shader_property_type::float_value: return "float";
    case shader_property_type::vector2_value: return "vector2";
    case shader_property_type::vector3_value: return "vector3";
    case shader_property_type::vector4_value: return "vector4";
    case shader_property_type::color_value: return "color";
    case shader_property_type::texture_value: return "texture2d";
  }

  return "float";
}

[[nodiscard]] inline auto shader_property_type_from_string(std::string_view value) -> shader_property_type {
  if (value == "vector2") return shader_property_type::vector2_value;
  if (value == "vector3") return shader_property_type::vector3_value;
  if (value == "vector4") return shader_property_type::vector4_value;
  if (value == "color") return shader_property_type::color_value;
  if (value == "texture2d") return shader_property_type::texture_value;
  return shader_property_type::float_value;
}

enum class shader_property_float_mode : std::uint8_t {
  plain,
  slider
}; // enum class shader_property_float_mode

/**
 * @brief A named material input: a shader graph's blackboard property or a field of a code shader's `properties` struct.
 *
 * For a texture, `slot` indexes generic_textures. For a value, it's a float offset into generic_params seen as one float array.
 * A graph packs a Float anywhere, a Vector2 at an even offset and everything else at a multiple of 4, assigning the slot once when the property is created so it survives renames and reordering.
 * A code shader's value slots are the float count of the value fields before it, so a value may straddle two float4s; shader_property_write/read and the generated load_properties handle that.
 * `id` is the graph's blackboard id, or the field's position plus one for a code shader.
 */
struct shader_property {
  std::uint32_t id{shader_property_invalid_id};
  std::string name{};
  std::string reference{};
  shader_property_type type{shader_property_type::float_value};
  shader_property_value value{};
  bool exposed{true};
  std::uint32_t slot{0u};
  shader_property_float_mode float_mode{shader_property_float_mode::plain};
  math::vector2 slider_range{0.0f, 1.0f};
  bool is_hdr{false};
  bool is_linear{false};
  bool is_texture_array{false}; // code shaders only: a Texture2DArray field, which only C# can fill
}; // struct shader_property

/** @brief How many generic_params floats a value of @p type takes; 0 for textures. */
[[nodiscard]] inline auto shader_property_component_count(shader_property_type type) -> std::uint32_t {
  switch (type) {
    case shader_property_type::float_value: return 1u;
    case shader_property_type::vector2_value: return 2u;
    case shader_property_type::vector3_value: return 3u;
    case shader_property_type::vector4_value: return 4u;
    case shader_property_type::color_value: return 4u;
    case shader_property_type::texture_value: return 0u;
  }

  return 1u;
}

/** @brief Which generic_params floats @p property occupies, as a bit mask (bit i = float i); 0 for textures. */
[[nodiscard]] inline auto shader_property_component_mask(shader_property_type type, std::uint32_t slot) -> std::uint64_t {
  const auto count = shader_property_component_count(type);
  // Out-of-range slots (a hand-edited file) occupy nothing here; codegen validation rejects them.
  if (count == 0u || slot + count > 64u) {
    return 0u;
  }

  return ((std::uint64_t{1u} << count) - 1u) << slot;
}

[[nodiscard]] inline auto shader_property_value_to_vector4(const shader_property_value& value) -> math::vector4 {
  return std::visit([]<typename T>(const T& entry) -> math::vector4 {
    if constexpr (std::is_same_v<T, std::float_t>) return math::vector4{entry, 0.0f, 0.0f, 0.0f};
    else if constexpr (std::is_same_v<T, math::vector2>) return math::vector4{entry.x(), entry.y(), 0.0f, 0.0f};
    else if constexpr (std::is_same_v<T, math::vector3>) return math::vector4{entry.x(), entry.y(), entry.z(), 0.0f};
    else if constexpr (std::is_same_v<T, math::vector4>) return entry;
    else if constexpr (std::is_same_v<T, math::color>) return math::vector4{entry.r(), entry.g(), entry.b(), entry.a()};
    else return math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
  }, value);
}

/** @brief Writes the first component_count(@p type) components of @p value at float offset @p slot of @p params, leaving the floats around it alone. */
inline auto shader_property_write(std::span<math::vector4> params, shader_property_type type, std::uint32_t slot, const math::vector4& value) -> void {
  const auto count = shader_property_component_count(type);

  for (auto component = std::uint32_t{0u}; component < count; ++component) {
    const auto offset = slot + component;

    if (offset < params.size() * 4u) {
      params[offset / 4u][offset % 4u] = value[component];
    }
  }
}

/** @brief The inverse of shader_property_write; components past the type's width read 0. */
[[nodiscard]] inline auto shader_property_read(std::span<const math::vector4> params, shader_property_type type, std::uint32_t slot) -> math::vector4 {
  auto result = math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
  const auto count = shader_property_component_count(type);

  for (auto component = std::uint32_t{0u}; component < count; ++component) {
    const auto offset = slot + component;

    if (offset < params.size() * 4u) {
      result[component] = params[offset / 4u][offset % 4u];
    }
  }

  return result;
}

/**
 * @brief Every value property's default written into a generic_params array, for seeding a material when its shader is assigned.
 *
 * @param properties The shader's properties.
 *
 * @return The packed defaults; floats no property covers are 0.
 */
[[nodiscard]] inline auto shader_property_default_params(std::span<const shader_property> properties) -> std::array<math::vector4, material_max_params> {
  auto result = std::array<math::vector4, material_max_params>{};

  for (const auto& property : properties) {
    shader_property_write(result, property.type, property.slot, shader_property_value_to_vector4(property.value));
  }

  return result;
}

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SHADER_PROPERTY_HPP_
