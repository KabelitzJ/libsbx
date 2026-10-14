// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_SHADER_GRAPH_HPP_
#define LIBSBX_ASSETS_SHADER_GRAPH_HPP_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include <fmt/format.h>

#include <libsbx/math/color.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>
#include <libsbx/assets/texture2d.hpp>

namespace sbx::assets {

inline constexpr auto shader_graph_max_params = std::uint32_t{8u};
inline constexpr auto shader_graph_max_textures = std::uint32_t{16u};

// generic_params seen as one array of floats; value properties are packed into it by component.
inline constexpr auto shader_graph_max_param_components = shader_graph_max_params * 4u;

enum class shader_value_type : std::uint8_t {
  scalar,
  vector2,
  vector3,
  vector4,
  texture2d
}; // enum class shader_value_type

[[nodiscard]] inline auto shader_value_type_component_count(shader_value_type type) -> std::uint32_t {
  switch (type) {
    case shader_value_type::scalar: return 1u;
    case shader_value_type::vector2: return 2u;
    case shader_value_type::vector3: return 3u;
    case shader_value_type::vector4: return 4u;
    case shader_value_type::texture2d: return 1u;
  }

  return 1u;
}

[[nodiscard]] inline auto shader_value_type_to_slang(shader_value_type type) -> const char* {
  switch (type) {
    case shader_value_type::scalar: return "float";
    case shader_value_type::vector2: return "float2";
    case shader_value_type::vector3: return "float3";
    case shader_value_type::vector4: return "float4";
    case shader_value_type::texture2d: return "uint"; // a bindless texture index
  }

  return "float";
}

[[nodiscard]] inline auto shader_value_type_display_name(shader_value_type type) -> const char* {
  switch (type) {
    case shader_value_type::scalar: return "Float";
    case shader_value_type::vector2: return "Float2";
    case shader_value_type::vector3: return "Float3";
    case shader_value_type::vector4: return "Float4";
    case shader_value_type::texture2d: return "Texture2D";
  }

  return "Float";
}

enum class shader_node_type : std::uint8_t {
  input_uv,
  input_normal,
  input_view_dir,
  input_vertex_position,
  input_vertex_normal,
  input_vertex_tangent,
  camera_position,
  main_light_direction,
  main_light_color,
  time,
  delta_time,
  scene_depth,
  screen_position,
  input_world_position,
  constant_float,
  constant_vector2,
  constant_vector3,
  constant_vector4,
  constant_color,
  texture_sample,
  add,
  subtract,
  multiply,
  divide,
  lerp,
  dot,
  cross,
  normalize,
  saturate,
  pow,
  step,
  smoothstep,
  negate,
  one_minus,
  absolute,
  floor,
  ceiling,
  round,
  fraction,
  sign,
  sine,
  cosine,
  minimum,
  maximum,
  clamp,
  length,
  distance,
  reflect,
  remap,
  fresnel_effect,
  simple_noise,
  voronoi,
  swizzle,
  split,
  combine,
  property,
  output_vertex,
  output_fragment_lit,
  output_fragment_unlit
}; // enum class shader_node_type

[[nodiscard]] inline auto shader_node_is_fragment_output(shader_node_type type) -> bool {
  return type == shader_node_type::output_fragment_lit || type == shader_node_type::output_fragment_unlit;
}

[[nodiscard]] inline auto shader_node_type_to_string(shader_node_type type) -> const char* {
  switch (type) {
    case shader_node_type::input_uv: return "input_uv";
    case shader_node_type::input_normal: return "input_normal";
    case shader_node_type::input_view_dir: return "input_view_dir";
    case shader_node_type::input_vertex_position: return "input_vertex_position";
    case shader_node_type::input_vertex_normal: return "input_vertex_normal";
    case shader_node_type::input_vertex_tangent: return "input_vertex_tangent";
    case shader_node_type::camera_position: return "camera_position";
    case shader_node_type::main_light_direction: return "main_light_direction";
    case shader_node_type::main_light_color: return "main_light_color";
    case shader_node_type::time: return "time";
    case shader_node_type::delta_time: return "delta_time";
    case shader_node_type::scene_depth: return "scene_depth";
    case shader_node_type::screen_position: return "screen_position";
    case shader_node_type::input_world_position: return "input_world_position";
    case shader_node_type::constant_float: return "constant_float";
    case shader_node_type::constant_vector2: return "constant_vector2";
    case shader_node_type::constant_vector3: return "constant_vector3";
    case shader_node_type::constant_vector4: return "constant_vector4";
    case shader_node_type::constant_color: return "constant_color";
    case shader_node_type::texture_sample: return "texture_sample";
    case shader_node_type::add: return "add";
    case shader_node_type::subtract: return "subtract";
    case shader_node_type::multiply: return "multiply";
    case shader_node_type::divide: return "divide";
    case shader_node_type::lerp: return "lerp";
    case shader_node_type::dot: return "dot";
    case shader_node_type::cross: return "cross";
    case shader_node_type::normalize: return "normalize";
    case shader_node_type::saturate: return "saturate";
    case shader_node_type::pow: return "pow";
    case shader_node_type::step: return "step";
    case shader_node_type::smoothstep: return "smoothstep";
    case shader_node_type::negate: return "negate";
    case shader_node_type::one_minus: return "one_minus";
    case shader_node_type::absolute: return "absolute";
    case shader_node_type::floor: return "floor";
    case shader_node_type::ceiling: return "ceiling";
    case shader_node_type::round: return "round";
    case shader_node_type::fraction: return "fraction";
    case shader_node_type::sign: return "sign";
    case shader_node_type::sine: return "sine";
    case shader_node_type::cosine: return "cosine";
    case shader_node_type::minimum: return "minimum";
    case shader_node_type::maximum: return "maximum";
    case shader_node_type::clamp: return "clamp";
    case shader_node_type::length: return "length";
    case shader_node_type::distance: return "distance";
    case shader_node_type::reflect: return "reflect";
    case shader_node_type::remap: return "remap";
    case shader_node_type::fresnel_effect: return "fresnel_effect";
    case shader_node_type::simple_noise: return "simple_noise";
    case shader_node_type::voronoi: return "voronoi";
    case shader_node_type::swizzle: return "swizzle";
    case shader_node_type::split: return "split";
    case shader_node_type::combine: return "combine";
    case shader_node_type::property: return "property";
    case shader_node_type::output_vertex: return "output_vertex";
    case shader_node_type::output_fragment_lit: return "output_fragment_lit";
    case shader_node_type::output_fragment_unlit: return "output_fragment_unlit";
  }

  return "constant_float";
}

[[nodiscard]] inline auto shader_node_type_from_string(std::string_view value) -> shader_node_type {
  if (value == "input_uv") return shader_node_type::input_uv;
  if (value == "input_normal") return shader_node_type::input_normal;
  if (value == "input_view_dir") return shader_node_type::input_view_dir;
  if (value == "input_vertex_position") return shader_node_type::input_vertex_position;
  if (value == "input_vertex_normal") return shader_node_type::input_vertex_normal;
  if (value == "input_vertex_tangent") return shader_node_type::input_vertex_tangent;
  if (value == "camera_position") return shader_node_type::camera_position;
  if (value == "main_light_direction") return shader_node_type::main_light_direction;
  if (value == "main_light_color") return shader_node_type::main_light_color;
  if (value == "time") return shader_node_type::time;
  if (value == "delta_time") return shader_node_type::delta_time;
  if (value == "scene_depth") return shader_node_type::scene_depth;
  if (value == "screen_position") return shader_node_type::screen_position;
  if (value == "input_world_position") return shader_node_type::input_world_position;
  if (value == "constant_float") return shader_node_type::constant_float;
  if (value == "constant_vector2") return shader_node_type::constant_vector2;
  if (value == "constant_vector3") return shader_node_type::constant_vector3;
  if (value == "constant_vector4") return shader_node_type::constant_vector4;
  if (value == "constant_color") return shader_node_type::constant_color;
  if (value == "texture_sample") return shader_node_type::texture_sample;
  if (value == "add") return shader_node_type::add;
  if (value == "subtract") return shader_node_type::subtract;
  if (value == "multiply") return shader_node_type::multiply;
  if (value == "divide") return shader_node_type::divide;
  if (value == "lerp") return shader_node_type::lerp;
  if (value == "dot") return shader_node_type::dot;
  if (value == "cross") return shader_node_type::cross;
  if (value == "normalize") return shader_node_type::normalize;
  if (value == "saturate") return shader_node_type::saturate;
  if (value == "pow") return shader_node_type::pow;
  if (value == "step") return shader_node_type::step;
  if (value == "smoothstep") return shader_node_type::smoothstep;
  if (value == "negate") return shader_node_type::negate;
  if (value == "one_minus") return shader_node_type::one_minus;
  if (value == "absolute") return shader_node_type::absolute;
  if (value == "floor") return shader_node_type::floor;
  if (value == "ceiling") return shader_node_type::ceiling;
  if (value == "round") return shader_node_type::round;
  if (value == "fraction") return shader_node_type::fraction;
  if (value == "sign") return shader_node_type::sign;
  if (value == "sine") return shader_node_type::sine;
  if (value == "cosine") return shader_node_type::cosine;
  if (value == "minimum") return shader_node_type::minimum;
  if (value == "maximum") return shader_node_type::maximum;
  if (value == "clamp") return shader_node_type::clamp;
  if (value == "length") return shader_node_type::length;
  if (value == "distance") return shader_node_type::distance;
  if (value == "reflect") return shader_node_type::reflect;
  if (value == "remap") return shader_node_type::remap;
  if (value == "fresnel_effect") return shader_node_type::fresnel_effect;
  if (value == "simple_noise") return shader_node_type::simple_noise;
  if (value == "voronoi") return shader_node_type::voronoi;
  if (value == "swizzle") return shader_node_type::swizzle;
  if (value == "split") return shader_node_type::split;
  if (value == "combine") return shader_node_type::combine;
  if (value == "property") return shader_node_type::property;
  if (value == "output_vertex") return shader_node_type::output_vertex;
  if (value == "output_fragment_lit") return shader_node_type::output_fragment_lit;
  if (value == "output_fragment_unlit") return shader_node_type::output_fragment_unlit;
  return shader_node_type::constant_float;
}

[[nodiscard]] inline auto shader_node_display_name(shader_node_type type) -> const char* {
  switch (type) {
    case shader_node_type::input_uv: return "UV";
    case shader_node_type::input_normal: return "Normal";
    case shader_node_type::input_view_dir: return "View Direction";
    case shader_node_type::input_vertex_position: return "Vertex Position";
    case shader_node_type::input_vertex_normal: return "Vertex Normal";
    case shader_node_type::input_vertex_tangent: return "Vertex Tangent";
    case shader_node_type::camera_position: return "Camera Position";
    case shader_node_type::main_light_direction: return "Main Light Direction";
    case shader_node_type::main_light_color: return "Main Light Color";
    case shader_node_type::time: return "Time";
    case shader_node_type::delta_time: return "Delta Time";
    case shader_node_type::scene_depth: return "Scene Depth";
    case shader_node_type::screen_position: return "Screen Position";
    case shader_node_type::input_world_position: return "World Position";
    case shader_node_type::constant_float: return "Float";
    case shader_node_type::constant_vector2: return "Vector2";
    case shader_node_type::constant_vector3: return "Vector3";
    case shader_node_type::constant_vector4: return "Vector4";
    case shader_node_type::constant_color: return "Color";
    case shader_node_type::texture_sample: return "Sample Texture";
    case shader_node_type::add: return "Add";
    case shader_node_type::subtract: return "Subtract";
    case shader_node_type::multiply: return "Multiply";
    case shader_node_type::divide: return "Divide";
    case shader_node_type::lerp: return "Lerp";
    case shader_node_type::dot: return "Dot";
    case shader_node_type::cross: return "Cross";
    case shader_node_type::normalize: return "Normalize";
    case shader_node_type::saturate: return "Saturate";
    case shader_node_type::pow: return "Power";
    case shader_node_type::step: return "Step";
    case shader_node_type::smoothstep: return "Smoothstep";
    case shader_node_type::negate: return "Negate";
    case shader_node_type::one_minus: return "One Minus";
    case shader_node_type::absolute: return "Absolute";
    case shader_node_type::floor: return "Floor";
    case shader_node_type::ceiling: return "Ceiling";
    case shader_node_type::round: return "Round";
    case shader_node_type::fraction: return "Fraction";
    case shader_node_type::sign: return "Sign";
    case shader_node_type::sine: return "Sine";
    case shader_node_type::cosine: return "Cosine";
    case shader_node_type::minimum: return "Minimum";
    case shader_node_type::maximum: return "Maximum";
    case shader_node_type::clamp: return "Clamp";
    case shader_node_type::length: return "Length";
    case shader_node_type::distance: return "Distance";
    case shader_node_type::reflect: return "Reflect";
    case shader_node_type::remap: return "Remap";
    case shader_node_type::fresnel_effect: return "Fresnel Effect";
    case shader_node_type::simple_noise: return "Simple Noise";
    case shader_node_type::voronoi: return "Voronoi";
    case shader_node_type::swizzle: return "Swizzle";
    case shader_node_type::split: return "Split";
    case shader_node_type::combine: return "Combine";
    case shader_node_type::property: return "Property";
    case shader_node_type::output_vertex: return "Vertex";
    case shader_node_type::output_fragment_lit: return "Fragment (Lit)";
    case shader_node_type::output_fragment_unlit: return "Fragment (Unlit)";
  }

  return "?";
}

enum class shader_node_category : std::uint8_t {
  input,
  constant,
  texture,
  basic,
  round,
  interpolation,
  range,
  trigonometry,
  vector,
  noise,
  channel,
  property,
  output
}; // enum class shader_node_category

[[nodiscard]] inline auto shader_node_category_of(shader_node_type type) -> shader_node_category {
  switch (type) {
    case shader_node_type::input_uv:
    case shader_node_type::input_normal:
    case shader_node_type::input_view_dir:
    case shader_node_type::input_vertex_position:
    case shader_node_type::input_vertex_normal:
    case shader_node_type::input_vertex_tangent:
    case shader_node_type::camera_position:
    case shader_node_type::main_light_direction:
    case shader_node_type::main_light_color:
    case shader_node_type::time:
    case shader_node_type::delta_time:
    case shader_node_type::scene_depth:
    case shader_node_type::screen_position:
    case shader_node_type::input_world_position:
      return shader_node_category::input;
    case shader_node_type::constant_float:
    case shader_node_type::constant_vector2:
    case shader_node_type::constant_vector3:
    case shader_node_type::constant_vector4:
    case shader_node_type::constant_color:
      return shader_node_category::constant;
    case shader_node_type::texture_sample:
      return shader_node_category::texture;
    case shader_node_type::add:
    case shader_node_type::subtract:
    case shader_node_type::multiply:
    case shader_node_type::divide:
    case shader_node_type::pow:
    case shader_node_type::negate:
    case shader_node_type::one_minus:
    case shader_node_type::absolute:
      return shader_node_category::basic;
    case shader_node_type::floor:
    case shader_node_type::ceiling:
    case shader_node_type::round:
    case shader_node_type::fraction:
    case shader_node_type::sign:
    case shader_node_type::step:
      return shader_node_category::round;
    case shader_node_type::lerp:
    case shader_node_type::smoothstep:
    case shader_node_type::remap:
      return shader_node_category::interpolation;
    case shader_node_type::clamp:
    case shader_node_type::saturate:
    case shader_node_type::minimum:
    case shader_node_type::maximum:
      return shader_node_category::range;
    case shader_node_type::sine:
    case shader_node_type::cosine:
      return shader_node_category::trigonometry;
    case shader_node_type::dot:
    case shader_node_type::cross:
    case shader_node_type::normalize:
    case shader_node_type::length:
    case shader_node_type::distance:
    case shader_node_type::reflect:
    case shader_node_type::fresnel_effect:
      return shader_node_category::vector;
    case shader_node_type::simple_noise:
    case shader_node_type::voronoi:
      return shader_node_category::noise;
    case shader_node_type::swizzle:
    case shader_node_type::split:
    case shader_node_type::combine:
      return shader_node_category::channel;
    case shader_node_type::property:
      return shader_node_category::property;
    case shader_node_type::output_vertex:
    case shader_node_type::output_fragment_lit:
    case shader_node_type::output_fragment_unlit:
      return shader_node_category::output;
  }

  return shader_node_category::basic;
}

[[nodiscard]] inline auto shader_node_has_output(shader_node_type type) -> bool {
  return type != shader_node_type::output_vertex && type != shader_node_type::output_fragment_lit && type != shader_node_type::output_fragment_unlit;
}

[[nodiscard]] inline auto shader_node_output_count(shader_node_type type) -> std::size_t {
  if (!shader_node_has_output(type)) {
    return 0u;
  }

  if (type == shader_node_type::texture_sample) {
    return 5u;
  }

  if (type == shader_node_type::split) {
    return 4u;
  }

  if (type == shader_node_type::combine) {
    return 3u;
  }

  if (type == shader_node_type::voronoi) {
    return 2u;
  }

  return 1u;
}

[[nodiscard]] inline auto shader_node_output_label(shader_node_type type, std::size_t pin) -> const char* {
  if (type == shader_node_type::texture_sample) {
    switch (pin) {
      case 0u: return "RGBA";
      case 1u: return "R";
      case 2u: return "G";
      case 3u: return "B";
      default: return "A";
    }
  }

  if (type == shader_node_type::split) {
    switch (pin) {
      case 0u: return "R";
      case 1u: return "G";
      case 2u: return "B";
      default: return "A";
    }
  }

  if (type == shader_node_type::combine) {
    switch (pin) {
      case 0u: return "RG";
      case 1u: return "RGB";
      default: return "RGBA";
    }
  }

  if (type == shader_node_type::voronoi) {
    return pin == 0u ? "Out" : "Cells";
  }

  return "Out";
}

[[nodiscard]] inline auto shader_node_input_count(shader_node_type type) -> std::size_t {
  switch (type) {
    case shader_node_type::input_uv:
    case shader_node_type::input_normal:
    case shader_node_type::input_view_dir:
    case shader_node_type::input_vertex_position:
    case shader_node_type::input_vertex_normal:
    case shader_node_type::input_vertex_tangent:
    case shader_node_type::camera_position:
    case shader_node_type::main_light_direction:
    case shader_node_type::main_light_color:
    case shader_node_type::time:
    case shader_node_type::delta_time:
    case shader_node_type::screen_position:
    case shader_node_type::input_world_position:
    case shader_node_type::constant_float:
    case shader_node_type::constant_vector2:
    case shader_node_type::constant_vector3:
    case shader_node_type::constant_vector4:
    case shader_node_type::constant_color:
    case shader_node_type::property:
      return 0u;
    case shader_node_type::scene_depth:
    case shader_node_type::normalize:
    case shader_node_type::saturate:
    case shader_node_type::negate:
    case shader_node_type::one_minus:
    case shader_node_type::absolute:
    case shader_node_type::floor:
    case shader_node_type::ceiling:
    case shader_node_type::round:
    case shader_node_type::fraction:
    case shader_node_type::sign:
    case shader_node_type::sine:
    case shader_node_type::cosine:
    case shader_node_type::length:
    case shader_node_type::swizzle:
    case shader_node_type::split:
      return 1u;
    case shader_node_type::add:
    case shader_node_type::subtract:
    case shader_node_type::multiply:
    case shader_node_type::divide:
    case shader_node_type::dot:
    case shader_node_type::cross:
    case shader_node_type::pow:
    case shader_node_type::step:
    case shader_node_type::minimum:
    case shader_node_type::maximum:
    case shader_node_type::distance:
    case shader_node_type::reflect:
    case shader_node_type::simple_noise:
    case shader_node_type::texture_sample:
      return 2u;
    case shader_node_type::lerp:
    case shader_node_type::smoothstep:
    case shader_node_type::clamp:
    case shader_node_type::remap:
    case shader_node_type::fresnel_effect:
    case shader_node_type::voronoi:
    case shader_node_type::output_vertex:
    case shader_node_type::output_fragment_unlit:
      return 3u;
    case shader_node_type::combine:
      return 4u;
    case shader_node_type::output_fragment_lit:
      return 8u;
  }

  return 0u;
}

[[nodiscard]] inline auto shader_node_input_label(shader_node_type type, std::size_t pin) -> const char* {
  switch (type) {
    case shader_node_type::texture_sample: return pin == 0u ? "UV" : "Texture";
    case shader_node_type::scene_depth: return "UV";
    case shader_node_type::normalize: return "In";
    case shader_node_type::saturate: return "In";
    case shader_node_type::swizzle: return "In";
    case shader_node_type::split: return "In";
    case shader_node_type::combine:
      switch (pin) {
        case 0u: return "R";
        case 1u: return "G";
        case 2u: return "B";
        default: return "A";
      }
    case shader_node_type::output_vertex:
      if (pin == 0u) return "Position";
      if (pin == 1u) return "Normal";
      return "Tangent";
    case shader_node_type::output_fragment_unlit:
      if (pin == 0u) return "Color";
      if (pin == 1u) return "Alpha";
      return "Alpha Clip Threshold";
    case shader_node_type::output_fragment_lit:
      switch (pin) {
        case 0u: return "Albedo";
        case 1u: return "Normal";
        case 2u: return "Metallic";
        case 3u: return "Roughness";
        case 4u: return "Emission";
        case 5u: return "Occlusion";
        case 6u: return "Alpha";
        default: return "Alpha Clip Threshold";
      }
    case shader_node_type::add:
    case shader_node_type::subtract:
    case shader_node_type::multiply:
    case shader_node_type::divide:
    case shader_node_type::dot:
    case shader_node_type::cross:
    case shader_node_type::pow:
    case shader_node_type::minimum:
    case shader_node_type::maximum:
    case shader_node_type::distance:
      return pin == 0u ? "A" : "B";
    case shader_node_type::step:
      return pin == 0u ? "Edge" : "X";
    case shader_node_type::lerp:
      if (pin == 0u) return "A";
      if (pin == 1u) return "B";
      return "T";
    case shader_node_type::smoothstep:
      if (pin == 0u) return "Edge0";
      if (pin == 1u) return "Edge1";
      return "X";
    case shader_node_type::clamp:
      if (pin == 0u) return "In";
      return pin == 1u ? "Min" : "Max";
    case shader_node_type::reflect:
      return pin == 0u ? "In" : "Normal";
    case shader_node_type::remap:
      switch (pin) {
        case 0u: return "In";
        case 1u: return "In Min Max";
        default: return "Out Min Max";
      }
    case shader_node_type::fresnel_effect:
      if (pin == 0u) return "Normal";
      if (pin == 1u) return "View Dir";
      return "Power";
    case shader_node_type::simple_noise:
      return pin == 0u ? "UV" : "Scale";
    case shader_node_type::voronoi:
      if (pin == 0u) return "UV";
      if (pin == 1u) return "Angle Offset";
      return "Cell Density";
    default:
      return "In";
  }
}

using shader_graph_node_value = std::variant<std::monostate, std::float_t, math::vector2, math::vector3, math::vector4, math::color, texture2d_handle, std::string>;

inline constexpr auto shader_graph_invalid_property_id = std::uint32_t{0u};

struct shader_graph_node {
  std::uint32_t id{0u};
  shader_node_type type{shader_node_type::constant_float};
  math::vector2 editor_position{0.0f, 0.0f};
  std::string name{};
  bool preview{false};
  shader_graph_node_value value{};
  std::uint32_t property_id{shader_graph_invalid_property_id}; // property nodes only
}; // struct shader_graph_node

[[nodiscard]] inline auto shader_node_swizzle_pattern(const shader_graph_node& node) -> std::string {
  const auto pattern = std::holds_alternative<std::string>(node.value) ? std::get<std::string>(node.value) : std::string{};
  return pattern.size() == 4u ? pattern : std::string{"rgba"};
}

[[nodiscard]] inline auto shader_node_scene_depth_mode(const shader_graph_node& node) -> std::string {
  const auto mode = std::holds_alternative<std::string>(node.value) ? std::get<std::string>(node.value) : std::string{};
  return (mode == "raw" || mode == "eye" || mode == "linear01") ? mode : std::string{"linear01"};
}

[[nodiscard]] inline auto shader_node_screen_position_mode(const shader_graph_node& node) -> std::string {
  const auto mode = std::holds_alternative<std::string>(node.value) ? std::get<std::string>(node.value) : std::string{};
  return (mode == "raw" || mode == "default") ? mode : std::string{"default"};
}

struct shader_graph_edge {
  std::uint32_t from_node{0u};
  std::uint32_t from_pin{0u};
  std::uint32_t to_node{0u};
  std::uint32_t to_pin{0u};
}; // struct shader_graph_edge

[[nodiscard]] inline auto shader_node_fixed_output_type(shader_node_type type, std::size_t pin) -> std::optional<shader_value_type> {
  switch (type) {
    case shader_node_type::input_uv:
    case shader_node_type::constant_vector2:
      return shader_value_type::vector2;
    case shader_node_type::input_normal:
    case shader_node_type::input_view_dir:
    case shader_node_type::input_vertex_position:
    case shader_node_type::input_vertex_normal:
    case shader_node_type::input_vertex_tangent:
    case shader_node_type::camera_position:
    case shader_node_type::main_light_direction:
    case shader_node_type::main_light_color:
    case shader_node_type::constant_vector3:
    case shader_node_type::cross:
    case shader_node_type::reflect:
    case shader_node_type::input_world_position:
      return shader_value_type::vector3;
    case shader_node_type::constant_float:
    case shader_node_type::dot:
    case shader_node_type::split:
    case shader_node_type::time:
    case shader_node_type::delta_time:
    case shader_node_type::scene_depth:
    case shader_node_type::length:
    case shader_node_type::distance:
    case shader_node_type::fresnel_effect:
    case shader_node_type::simple_noise:
    case shader_node_type::voronoi:
      return shader_value_type::scalar;
    case shader_node_type::constant_vector4:
    case shader_node_type::constant_color:
    case shader_node_type::screen_position:
      return shader_value_type::vector4;
    case shader_node_type::texture_sample:
      return pin == 0u ? shader_value_type::vector4 : shader_value_type::scalar;
    case shader_node_type::combine:
      return pin == 0u ? shader_value_type::vector2 : pin == 1u ? shader_value_type::vector3 : shader_value_type::vector4;
    default:
      return std::nullopt;
  }
}

[[nodiscard]] inline auto shader_node_fixed_input_type(shader_node_type type, std::size_t pin) -> std::optional<shader_value_type> {
  switch (type) {
    case shader_node_type::texture_sample:
      return pin == 0u ? shader_value_type::vector2 : shader_value_type::texture2d;
    case shader_node_type::scene_depth:
      return shader_value_type::vector2;
    case shader_node_type::cross:
    case shader_node_type::reflect:
      return shader_value_type::vector3;
    case shader_node_type::combine:
      return shader_value_type::scalar;
    case shader_node_type::remap:
      if (pin == 0u) return std::nullopt;
      return shader_value_type::vector2;
    case shader_node_type::fresnel_effect:
      return pin == 2u ? shader_value_type::scalar : shader_value_type::vector3;
    case shader_node_type::simple_noise:
    case shader_node_type::voronoi:
      return pin == 0u ? shader_value_type::vector2 : shader_value_type::scalar;
    case shader_node_type::output_vertex:
      return shader_value_type::vector3;
    case shader_node_type::output_fragment_unlit:
      return pin == 0u ? shader_value_type::vector3 : shader_value_type::scalar;
    case shader_node_type::output_fragment_lit:
      switch (pin) {
        case 0u: return shader_value_type::vector3;
        case 1u: return shader_value_type::vector3;
        case 2u: return shader_value_type::scalar;
        case 3u: return shader_value_type::scalar;
        case 4u: return shader_value_type::vector3;
        case 5u: return shader_value_type::scalar;
        default: return shader_value_type::scalar;
      }
    default:
      return std::nullopt;
  }
}

enum class shader_graph_parameter_type : std::uint8_t {
  float_value,
  vector2_value,
  vector3_value,
  vector4_value,
  color_value,
  texture_value
}; // enum class shader_graph_parameter_type

[[nodiscard]] inline auto shader_graph_parameter_type_to_string(shader_graph_parameter_type type) -> const char* {
  switch (type) {
    case shader_graph_parameter_type::float_value: return "float";
    case shader_graph_parameter_type::vector2_value: return "vector2";
    case shader_graph_parameter_type::vector3_value: return "vector3";
    case shader_graph_parameter_type::vector4_value: return "vector4";
    case shader_graph_parameter_type::color_value: return "color";
    case shader_graph_parameter_type::texture_value: return "texture2d";
  }

  return "float";
}

[[nodiscard]] inline auto shader_graph_parameter_type_from_string(std::string_view value) -> shader_graph_parameter_type {
  if (value == "vector2") return shader_graph_parameter_type::vector2_value;
  if (value == "vector3") return shader_graph_parameter_type::vector3_value;
  if (value == "vector4") return shader_graph_parameter_type::vector4_value;
  if (value == "color") return shader_graph_parameter_type::color_value;
  if (value == "texture2d") return shader_graph_parameter_type::texture_value;
  return shader_graph_parameter_type::float_value;
}

[[nodiscard]] inline auto shader_graph_parameter_value_type(shader_graph_parameter_type type) -> shader_value_type {
  switch (type) {
    case shader_graph_parameter_type::float_value: return shader_value_type::scalar;
    case shader_graph_parameter_type::vector2_value: return shader_value_type::vector2;
    case shader_graph_parameter_type::vector3_value: return shader_value_type::vector3;
    case shader_graph_parameter_type::vector4_value: return shader_value_type::vector4;
    case shader_graph_parameter_type::color_value: return shader_value_type::vector4;
    case shader_graph_parameter_type::texture_value: return shader_value_type::texture2d;
  }

  return shader_value_type::scalar;
}

enum class shader_graph_float_mode : std::uint8_t {
  plain,
  slider
}; // enum class shader_graph_float_mode

/**
 * @brief A blackboard property: a named material input that property nodes read.
 *
 * For a texture, `slot` indexes generic_textures. For a value, it's a float offset into generic_params seen as one float array, packed:
 * a Float goes anywhere, a Vector2 at an even offset, everything else at a multiple of 4, so no value straddles two float4s.
 * Either is assigned once, when the property is created, and survives renames and reordering, since materials store their values by position.
 */
struct shader_graph_property {
  std::uint32_t id{shader_graph_invalid_property_id};
  std::string name{};
  std::string reference{};
  shader_graph_parameter_type type{shader_graph_parameter_type::float_value};
  shader_graph_node_value value{};
  bool exposed{true};
  std::uint32_t slot{0u};
  shader_graph_float_mode float_mode{shader_graph_float_mode::plain};
  math::vector2 slider_range{0.0f, 1.0f};
  bool is_hdr{false};
  bool is_linear{false};
}; // struct shader_graph_property

/** @brief One material input a graph reads: a property, or an inline Sample Texture whose Texture input is unconnected (property_id invalid, never exposed). `slot` as in shader_graph_property. */
struct shader_graph_parameter {
  std::uint32_t property_id{shader_graph_invalid_property_id};
  std::uint32_t node_id{0u};
  std::string name{};
  std::string reference{};
  shader_graph_parameter_type type{shader_graph_parameter_type::float_value};
  std::uint32_t slot{0u};
  bool exposed{false};
  shader_graph_node_value default_value{};
}; // struct shader_graph_parameter

[[nodiscard]] inline auto shader_graph_find_property(const std::vector<shader_graph_property>& properties, std::uint32_t id) -> memory::observer_ptr<const shader_graph_property> {
  const auto entry = std::ranges::find(properties, id, &shader_graph_property::id);
  return entry != properties.end() ? memory::make_observer(*entry) : memory::observer_ptr<const shader_graph_property>{};
}

/** @brief How many generic_params floats a value of @p type takes; 0 for textures. */
[[nodiscard]] inline auto shader_graph_parameter_component_count(shader_graph_parameter_type type) -> std::uint32_t {
  switch (type) {
    case shader_graph_parameter_type::float_value: return 1u;
    case shader_graph_parameter_type::vector2_value: return 2u;
    case shader_graph_parameter_type::vector3_value: return 3u;
    case shader_graph_parameter_type::vector4_value: return 4u;
    case shader_graph_parameter_type::color_value: return 4u;
    case shader_graph_parameter_type::texture_value: return 0u;
  }

  return 1u;
}

[[nodiscard]] inline auto shader_graph_parameter_alignment(shader_graph_parameter_type type) -> std::uint32_t {
  switch (type) {
    case shader_graph_parameter_type::float_value: return 1u;
    case shader_graph_parameter_type::vector2_value: return 2u;
    default: return 4u;
  }
}

/** @brief Which generic_params floats @p property occupies, as a bit mask (bit i = float i); 0 for textures. */
[[nodiscard]] inline auto shader_graph_property_component_mask(shader_graph_parameter_type type, std::uint32_t slot) -> std::uint64_t {
  const auto count = shader_graph_parameter_component_count(type);
  // Out-of-range slots (a hand-edited file) occupy nothing here; codegen validation rejects them.
  if (count == 0u || slot + count > 64u) {
    return 0u;
  }

  return ((std::uint64_t{1u} << count) - 1u) << slot;
}

/**
 * @brief Where a new @p type property goes: the lowest free texture slot, or the lowest aligned float offset whose components are all free.
 *
 * At or past the budget (shader_graph_max_textures / shader_graph_max_param_components) when the graph is full.
 */
[[nodiscard]] inline auto shader_graph_next_property_slot(const std::vector<shader_graph_property>& properties, shader_graph_parameter_type type) -> std::uint32_t {
  if (type == shader_graph_parameter_type::texture_value) {
    for (auto slot = std::uint32_t{0u}; ; ++slot) {
      const auto taken = std::ranges::any_of(properties, [slot](const auto& property) { return property.type == shader_graph_parameter_type::texture_value && property.slot == slot; });

      if (!taken) {
        return slot;
      }
    }
  }

  auto used = std::uint64_t{0u};

  for (const auto& property : properties) {
    used |= shader_graph_property_component_mask(property.type, property.slot);
  }

  const auto alignment = shader_graph_parameter_alignment(type);

  for (auto offset = std::uint32_t{0u}; ; offset += alignment) {
    if ((shader_graph_property_component_mask(type, offset) & used) == 0u) {
      return offset;
    }
  }
}

/** @brief Whether a new @p type property still fits the material's budget. */
[[nodiscard]] inline auto shader_graph_has_room_for(const std::vector<shader_graph_property>& properties, shader_graph_parameter_type type) -> bool {
  const auto slot = shader_graph_next_property_slot(properties, type);

  return type == shader_graph_parameter_type::texture_value
    ? slot < shader_graph_max_textures
    : slot + shader_graph_parameter_component_count(type) <= shader_graph_max_param_components;
}

[[nodiscard]] inline auto shader_graph_next_property_id(const std::vector<shader_graph_property>& properties) -> std::uint32_t {
  auto next = std::uint32_t{1u};

  for (const auto& property : properties) {
    next = std::max(next, property.id + 1u);
  }

  return next;
}

/** @brief A unique reference name derived from @p name, Unity-style: "Base Color" -> "_BaseColor", "_BaseColor_1" if taken. */
[[nodiscard]] inline auto shader_graph_make_reference(std::string_view name, const std::vector<shader_graph_property>& properties, std::uint32_t ignored_id = shader_graph_invalid_property_id) -> std::string {
  auto base = std::string{"_"};
  auto capitalize = true;

  for (const auto c : name) {
    const auto is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    const auto is_digit = c >= '0' && c <= '9';

    if (!is_alpha && !is_digit) {
      capitalize = true;
      continue;
    }

    if (is_digit && base.size() == 1u) {
      base += 'P'; // a reference can't start with a digit
    }

    base += (capitalize && c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    capitalize = false;
  }

  if (base.size() == 1u) {
    base += "Property";
  }

  const auto is_taken = [&](const std::string& candidate) {
    return std::ranges::any_of(properties, [&](const auto& property) { return property.id != ignored_id && property.reference == candidate; });
  };

  auto result = base;

  for (auto suffix = 1u; is_taken(result); ++suffix) {
    result = fmt::format("{}_{}", base, suffix);
  }

  return result;
}

/** @brief The Slang expression reading a value property's packed components out of `material.generic_params`. */
[[nodiscard]] inline auto shader_graph_parameter_expression(shader_graph_parameter_type type, std::uint32_t slot) -> std::string {
  const auto count = shader_graph_parameter_component_count(type);

  if (count == 4u) {
    return fmt::format("material.generic_params[{}]", slot / 4u);
  }

  return fmt::format("material.generic_params[{}].{}", slot / 4u, std::string_view{"xyzw"}.substr(slot % 4u, count));
}

[[nodiscard]] inline auto shader_graph_value_to_vector4(const shader_graph_node_value& value) -> math::vector4 {
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
inline auto shader_graph_write_parameter(std::span<math::vector4> params, shader_graph_parameter_type type, std::uint32_t slot, const math::vector4& value) -> void {
  const auto count = shader_graph_parameter_component_count(type);

  for (auto component = std::uint32_t{0u}; component < count; ++component) {
    const auto offset = slot + component;

    if (offset < params.size() * 4u) {
      params[offset / 4u][offset % 4u] = value[component];
    }
  }
}

/** @brief The inverse of shader_graph_write_parameter; components past the type's width read 0. */
[[nodiscard]] inline auto shader_graph_read_parameter(std::span<const math::vector4> params, shader_graph_parameter_type type, std::uint32_t slot) -> math::vector4 {
  auto result = math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
  const auto count = shader_graph_parameter_component_count(type);

  for (auto component = std::uint32_t{0u}; component < count; ++component) {
    const auto offset = slot + component;

    if (offset < params.size() * 4u) {
      result[component] = params[offset / 4u][offset % 4u];
    }
  }

  return result;
}

/**
 * @brief Every material slot the graph reads: all properties (by slot), then inline Sample Texture nodes on the texture slots no property uses, in node id order.
 *
 * Inline slots aren't stable, which is fine: they're never exposed, and asset_residency fills them from the graph's own default.
 */
[[nodiscard]] inline auto compute_shader_graph_parameters(const std::vector<shader_graph_property>& properties, const std::vector<shader_graph_node>& nodes, const std::vector<shader_graph_edge>& edges) -> std::vector<shader_graph_parameter> {
  auto result = std::vector<shader_graph_parameter>{};
  result.reserve(properties.size());

  for (const auto& property : properties) {
    result.push_back(shader_graph_parameter{property.id, 0u, property.name, property.reference, property.type, property.slot, property.exposed, property.value});
  }

  auto inline_samples = std::vector<memory::observer_ptr<const shader_graph_node>>{};

  for (const auto& node : nodes) {
    const auto has_texture_input = std::ranges::any_of(edges, [&node](const auto& edge) { return edge.to_node == node.id && edge.to_pin == 1u; });

    if (node.type == shader_node_type::texture_sample && !has_texture_input) {
      inline_samples.push_back(memory::make_observer(node));
    }
  }

  std::ranges::sort(inline_samples, {}, [](const auto& node) { return node->id; });

  auto slot = std::uint32_t{0u};

  for (const auto& node : inline_samples) {
    while (std::ranges::any_of(properties, [slot](const auto& property) { return property.type == shader_graph_parameter_type::texture_value && property.slot == slot; })) {
      ++slot;
    }

    result.push_back(shader_graph_parameter{shader_graph_invalid_property_id, node->id, node->name, std::string{}, shader_graph_parameter_type::texture_value, slot++, false, node->value});
  }

  return result;
}

class shader_graph_type_resolver {

public:

  shader_graph_type_resolver(const std::vector<shader_graph_property>& properties, const std::vector<shader_graph_node>& nodes, const std::vector<shader_graph_edge>& edges)
  : _properties{memory::make_observer(properties)} {
    for (const auto& node : nodes) {
      _nodes_by_id.emplace(node.id, &node);
    }

    for (const auto& edge : edges) {
      _incoming.emplace(_key(edge.to_node, edge.to_pin), _key(edge.from_node, edge.from_pin));
    }
  }

  [[nodiscard]] auto output_type(std::uint32_t node_id, std::uint32_t pin = 0u) -> std::optional<shader_value_type> {
    const auto key = _key(node_id, pin);

    if (const auto cached = _memo.find(key); cached != _memo.end()) {
      return cached->second;
    }

    _memo.emplace(key, std::nullopt);

    const auto result = _resolve(node_id, pin);
    _memo[key] = result;
    return result;
  }

  [[nodiscard]] auto input_type(std::uint32_t node_id, std::uint32_t pin) -> std::optional<shader_value_type> {
    const auto entry = _nodes_by_id.find(node_id);

    if (entry == _nodes_by_id.end()) {
      return std::nullopt;
    }

    if (const auto fixed = shader_node_fixed_input_type(entry->second->type, pin)) {
      return fixed;
    }

    return _operating_type(node_id, std::nullopt);
  }

  [[nodiscard]] auto accepts(std::uint32_t node_id, std::uint32_t pin, std::optional<shader_value_type> source_type) -> bool {
    const auto entry = _nodes_by_id.find(node_id);

    if (entry == _nodes_by_id.end() || !source_type) {
      return entry != _nodes_by_id.end();
    }

    const auto type = entry->second->type;

    if (const auto fixed = shader_node_fixed_input_type(type, pin)) {
      return *fixed == *source_type || (*fixed == shader_value_type::vector3 && *source_type == shader_value_type::vector4);
    }

    // A texture only plugs into a fixed Texture pin.
    if (*source_type == shader_value_type::texture2d) {
      return false;
    }

    if (*source_type == shader_value_type::scalar) {
      return true;
    }

    const auto operating = _operating_type(node_id, pin);

    return !operating || *operating == *source_type || (*operating == shader_value_type::vector3 && *source_type == shader_value_type::vector4);
  }

private:

  [[nodiscard]] static auto _key(std::uint32_t node_id, std::uint32_t pin) -> std::uint64_t {
    return (static_cast<std::uint64_t>(node_id) << 32u) | pin;
  }

  [[nodiscard]] auto _resolve(std::uint32_t node_id, std::uint32_t pin) -> std::optional<shader_value_type> {
    const auto entry = _nodes_by_id.find(node_id);

    if (entry == _nodes_by_id.end()) {
      return std::nullopt;
    }

    if (entry->second->type == shader_node_type::property) {
      const auto property = shader_graph_find_property(*_properties, entry->second->property_id);
      return property ? std::optional{shader_graph_parameter_value_type(property->type)} : std::nullopt;
    }

    if (const auto fixed = shader_node_fixed_output_type(entry->second->type, pin)) {
      return fixed;
    }

    return _operating_type(node_id, std::nullopt);
  }

  [[nodiscard]] auto _operating_type(std::uint32_t node_id, std::optional<std::uint32_t> exclude_pin) -> std::optional<shader_value_type> {
    const auto entry = _nodes_by_id.find(node_id);

    if (entry == _nodes_by_id.end()) {
      return std::nullopt;
    }

    const auto dominant_pins = _dominant_pins(entry->second->type);

    auto examined_count = std::size_t{0u};
    auto scalar_count = std::size_t{0u};

    for (const auto pin : dominant_pins) {
      if (exclude_pin && pin == *exclude_pin) {
        continue;
      }

      const auto source = _incoming.find(_key(node_id, pin));

      if (source == _incoming.end()) {
        continue;
      }

      const auto resolved = output_type(static_cast<std::uint32_t>(source->second >> 32u), static_cast<std::uint32_t>(source->second));

      if (!resolved) {
        continue;
      }

      ++examined_count;

      if (*resolved != shader_value_type::scalar) {
        return resolved;
      }

      ++scalar_count;
    }

    if (examined_count > 0u && examined_count == dominant_pins.size() && scalar_count == examined_count) {
      return shader_value_type::scalar;
    }

    return std::nullopt;
  }

  // Static storage: a returned std::initializer_list would point at an array destroyed when the function returns.
  [[nodiscard]] static auto _dominant_pins(shader_node_type type) -> std::span<const std::uint32_t> {
    static constexpr auto both_inputs = std::array<std::uint32_t, 2u>{0u, 1u};
    static constexpr auto first_input = std::array<std::uint32_t, 1u>{0u};

    switch (type) {
      case shader_node_type::add:
      case shader_node_type::subtract:
      case shader_node_type::multiply:
      case shader_node_type::divide:
      case shader_node_type::step:
      case shader_node_type::lerp:
      case shader_node_type::smoothstep:
      case shader_node_type::dot:
      case shader_node_type::minimum:
      case shader_node_type::maximum:
      case shader_node_type::distance:
        return both_inputs;
      case shader_node_type::pow:
      case shader_node_type::normalize:
      case shader_node_type::saturate:
      case shader_node_type::swizzle:
      case shader_node_type::split:
      case shader_node_type::negate:
      case shader_node_type::one_minus:
      case shader_node_type::absolute:
      case shader_node_type::floor:
      case shader_node_type::ceiling:
      case shader_node_type::round:
      case shader_node_type::fraction:
      case shader_node_type::sign:
      case shader_node_type::sine:
      case shader_node_type::cosine:
      case shader_node_type::clamp:
      case shader_node_type::length:
      case shader_node_type::remap:
        return first_input;
      default:
        return {};
    }
  }

  memory::observer_ptr<const std::vector<shader_graph_property>> _properties{};
  std::unordered_map<std::uint32_t, const shader_graph_node*> _nodes_by_id{};
  std::unordered_map<std::uint64_t, std::uint64_t> _incoming{};
  std::unordered_map<std::uint64_t, std::optional<shader_value_type>> _memo{};

}; // class shader_graph_type_resolver


class shader_graph final : public loadable {

  friend class asset_residency;

public:

  struct create_info {
    std::string name{"shader_graph"};
    std::vector<shader_graph_property> properties{};
    std::vector<shader_graph_node> nodes{};
    std::vector<shader_graph_edge> edges{};
  }; // struct create_info

  shader_graph() = default;

  explicit shader_graph(const create_info& create_info)
  : _name{create_info.name},
    _properties{create_info.properties},
    _nodes{create_info.nodes},
    _edges{create_info.edges} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return !_nodes.empty();
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& {
    return _name;
  }

  [[nodiscard]] auto properties() const noexcept -> const std::vector<shader_graph_property>& {
    return _properties;
  }

  [[nodiscard]] auto nodes() const noexcept -> const std::vector<shader_graph_node>& {
    return _nodes;
  }

  [[nodiscard]] auto edges() const noexcept -> const std::vector<shader_graph_edge>& {
    return _edges;
  }

  [[nodiscard]] auto next_node_id() const noexcept -> std::uint32_t {
    auto next = std::uint32_t{0u};

    for (const auto& node : _nodes) {
      next = std::max(next, node.id + 1u);
    }

    return next;
  }

  [[nodiscard]] auto parameters() const -> std::vector<shader_graph_parameter> {
    return compute_shader_graph_parameters(_properties, _nodes, _edges);
  }

private:

  std::string _name{"shader_graph"};
  std::vector<shader_graph_property> _properties{};
  std::vector<shader_graph_node> _nodes{};
  std::vector<shader_graph_edge> _edges{};
  math::uuid _id{math::uuid::nil()};

}; // class shader_graph

using shader_graph_handle = asset_handle<shader_graph>;

[[nodiscard]] inline auto shader_graph_default_generic_params(const shader_graph& graph) -> std::array<math::vector4, shader_graph_max_params> {
  auto result = std::array<math::vector4, shader_graph_max_params>{};

  for (const auto& parameter : graph.parameters()) {
    if (parameter.type != shader_graph_parameter_type::texture_value) {
      shader_graph_write_parameter(result, parameter.type, parameter.slot, shader_graph_value_to_vector4(parameter.default_value));
    }
  }

  return result;
}

[[nodiscard]] inline auto shader_graph_default_generic_textures(const shader_graph& graph) -> std::array<texture2d_handle, shader_graph_max_textures> {
  auto result = std::array<texture2d_handle, shader_graph_max_textures>{};

  for (const auto& parameter : graph.parameters()) {
    if (parameter.type == shader_graph_parameter_type::texture_value && parameter.slot < result.size() && std::holds_alternative<texture2d_handle>(parameter.default_value)) {
      result[parameter.slot] = std::get<texture2d_handle>(parameter.default_value);
    }
  }

  return result;
}

[[nodiscard]] inline auto shader_graph_generated_name(const math::uuid& id, std::uint64_t generation) -> std::string {
  return fmt::format("shader_graph_{}_{}", id.value(), generation);
}

[[nodiscard]] inline auto shader_graph_generated_path(const math::uuid& id, std::uint64_t generation) -> std::string {
  return fmt::format("engine://shaders/generated/{}.slang", shader_graph_generated_name(id, generation));
}

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SHADER_GRAPH_HPP_
