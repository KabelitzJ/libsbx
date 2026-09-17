// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_SHADER_GRAPH_HPP_
#define LIBSBX_ASSETS_SHADER_GRAPH_HPP_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
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

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>
#include <libsbx/assets/texture.hpp>

namespace sbx::assets {

// Fixed cap on how many exposed constant_*/texture_sample parameters one graph can declare --
// matches material_data's generic_params[]/generic_textures[] array sizes (asset_residency.cpp,
// frame_data.slang) and material::create_info's own generic_params/generic_textures arrays. Shared
// here so every one of those stays in lockstep if this ever needs to grow.
inline constexpr auto shader_graph_max_params = std::uint32_t{8u};
inline constexpr auto shader_graph_max_textures = std::uint32_t{4u};

// A pin's value shape. Every pin has one, either fixed (a plain function of its node's type -- see
// shader_node_fixed_output_type/shader_node_fixed_input_type) or dynamic, resolved from whatever's
// actually wired up (shader_graph_type_resolver below) -- but never both, and never anything in
// between: a connection is only ever legal when the source's resolved type equals the target pin's,
// full stop, except that any pin whose requirement isn't fixed also always accepts a scalar (Slang's
// own native, lossless scalar<->vector broadcast -- not the silent truncation this system exists to
// prevent). Width conversion beyond that is what the Split/Combine nodes are for, explicitly.
enum class shader_value_type : std::uint8_t {
  scalar,
  vector2,
  vector3,
  vector4
}; // enum class shader_value_type

[[nodiscard]] inline auto shader_value_type_component_count(shader_value_type type) -> std::uint32_t {
  switch (type) {
    case shader_value_type::scalar: return 1u;
    case shader_value_type::vector2: return 2u;
    case shader_value_type::vector3: return 3u;
    case shader_value_type::vector4: return 4u;
  }

  return 1u;
}

[[nodiscard]] inline auto shader_value_type_to_slang(shader_value_type type) -> const char* {
  switch (type) {
    case shader_value_type::scalar: return "float";
    case shader_value_type::vector2: return "float2";
    case shader_value_type::vector3: return "float3";
    case shader_value_type::vector4: return "float4";
  }

  return "float";
}

[[nodiscard]] inline auto shader_value_type_display_name(shader_value_type type) -> const char* {
  switch (type) {
    case shader_value_type::scalar: return "Float";
    case shader_value_type::vector2: return "Float2";
    case shader_value_type::vector3: return "Float3";
    case shader_value_type::vector4: return "Float4";
  }

  return "Float";
}

// The engine's only built-in shading models are pbr/unlit (material::shading_model) -- everything
// else, a user composes as a shader_graph material. This is the node library a graph can be built
// from; see shader_graph_codegen.hpp for how each of these turns into Slang.
enum class shader_node_type : std::uint8_t {
  input_uv,
  input_normal,
  input_view_dir,
  input_vertex_position, // valid only reachable from output_vertex -- vertex-stage (object-space), not fragment
  input_vertex_normal,   // same restriction
  input_vertex_tangent,  // same restriction
  camera_position,       // world-space camera position (frame_data.camera_position) -- valid in either stage
  main_light_direction,  // directional light 0's own travel direction (light -> surface, matching Unity
                          // Shader Graph's own node -- negate it for N.L), or (0,-1,0) if the scene has none
  main_light_color,      // that same light's color*intensity (radiance), or black if the scene has none
  time,                  // seconds since the engine started
  delta_time,            // seconds since the previous frame
  scene_depth,           // UV (float2, optional -- default this fragment's own screen UV) -> scalar,
                          // whole-scene depth (matches Unity Shader Graph's own Scene Depth node);
                          // Raw/Eye/Linear01 sampling mode is a per-node setting (node.value, a
                          // string -- see shader_node_scene_depth_mode), not a pin. Fragment-only:
                          // there's no meaningful screen position in the vertex stage.
  constant_float,
  constant_vector2,
  constant_vector3,
  constant_vector4,
  constant_color,
  texture_sample,     // 1 input (uv, optional -- falls back to the calling stage's own uv if unconnected)
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
  negate,         // 1 input (any width), -x
  one_minus,      // 1 input (any width), 1-x
  absolute,       // 1 input (any width), abs(x)
  floor,          // 1 input (any width), floor(x)
  ceiling,        // 1 input (any width), ceil(x)
  round,          // 1 input (any width), round(x)
  fraction,       // 1 input (any width), frac(x)
  sign,           // 1 input (any width), sign(x)
  sine,           // 1 input (any width, radians), sin(x)
  cosine,         // 1 input (any width, radians), cos(x)
  minimum,        // 2 inputs (any matching width, or a scalar broadcast against the other), min(a,b)
  maximum,        // same shape, max(a,b)
  clamp,          // In (any width) + Min/Max (that same width, or a scalar broadcast), clamp(in,min,max)
  length,         // 1 input (any width) -> scalar, length(x)
  distance,       // 2 inputs (any matching width) -> scalar, distance(a,b)
  reflect,        // In + Normal (both fixed float3) -> float3, reflect(in,normal)
  remap,          // In (any width, dynamic) + In Min Max/Out Min Max (each a float2 range -- .x/.y, not
                   // two separate scalars, since they're always a matched pair -- each optional, default
                   // (0,1)) -> that same width, linearly remaps In from In Min Max to Out Min Max
  fresnel_effect, // Normal/View Dir (both fixed float3, each optional -- default this fragment's own N/V) +
                   // Power (scalar, optional -- default 1) -> scalar, pow(saturate(1-dot(N,V)), power). Fragment-only.
  simple_noise,   // UV (float2, optional -- default this stage's own uv) + Scale (scalar, optional, default 500) ->
                   // scalar, 3-octave value noise (matches Unity Shader Graph's own Simple Noise node)
  voronoi,        // UV (float2, optional -- default this stage's own uv) + Angle Offset (scalar, optional,
                   // default 2) + Cell Density (scalar, optional, default 5) -> 2 outputs, Out (nearest cell's
                   // distance) and Cells (that cell's random offset.x -- NOT a true per-cell id; matches Unity
                   // Shader Graph's own Voronoi node exactly, including that quirk)
  swizzle, // 1 input (any width), 1 output the SAME width as the input -- each of its up to 4 output
           // components independently picks which input component (R/G/B/A) feeds it, e.g. "bgra"
  split,   // 1 input (any width), always 4 outputs (R/G/B/A, always scalar) -- a component beyond the
           // input's own width reads as a literal 0, matching Unity Shader Graph's Split node exactly
  combine, // up to 4 inputs (R/G/B/A, always scalar, each defaulting to 0 if unconnected), always 3
           // outputs (RG/RGB/RGBA, fixed vector2/vector3/vector4) -- matching Unity's Combine node
  output_vertex,        // sink: Position/Normal/Tangent. Optional -- unconnected pins pass the vertex through unchanged.
  output_fragment_lit,   // sink: Albedo/Normal/Metallic/Roughness/Emission/Occlusion/Alpha/Alpha Clip Threshold -- feeds the standard PBR lighting.
  output_fragment_unlit  // sink: Color/Alpha/Alpha Clip Threshold, written out with no lighting at all.
}; // enum class shader_node_type

// Exactly one of these two is required per graph -- generalizes "which fragment output mode".
[[nodiscard]] inline auto shader_node_is_fragment_output(shader_node_type type) -> bool {
  return type == shader_node_type::output_fragment_lit || type == shader_node_type::output_fragment_unlit;
}

// Canonical string form, shared by YAML parse (asset_cooker_shader_graph.cpp), YAML save
// (asset_residency.cpp), and codegen's error messages (shader_graph_codegen.cpp) -- one mapping
// table instead of three hand-duplicated switches.
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
  if (value == "output_vertex") return shader_node_type::output_vertex;
  if (value == "output_fragment_lit") return shader_node_type::output_fragment_lit;
  if (value == "output_fragment_unlit") return shader_node_type::output_fragment_unlit;
  return shader_node_type::constant_float; // default -- also constant_float's own name, matching the enum's first/default value
}

// A human-readable label for a node's palette entry / canvas header -- distinct from
// shader_node_type_to_string (the YAML/identifier form), which stays a stable on-disk/generated-
// code token independent of any display wording change here.
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
    case shader_node_type::output_vertex: return "Vertex";
    case shader_node_type::output_fragment_lit: return "Fragment (Lit)";
    case shader_node_type::output_fragment_unlit: return "Fragment (Unlit)";
  }

  return "?";
}

// Node palette grouping, for the editor's "Add Node" menu -- purely a UI concern, no effect on
// codegen or serialization. What used to be one flat "Math" bucket is now split the way Unity
// Shader Graph's own node library splits it (Basic/Round/Interpolation/Range/Trigonometry/Vector/
// Channel), just trimmed to the subset of node kinds this graph actually has -- a single "Math"
// category stopped being browsable once there were 30+ node kinds in it.
enum class shader_node_category : std::uint8_t {
  input,
  constant,
  texture,
  basic,         // Add, Subtract, Multiply, Divide, Power, Negate, One Minus, Absolute
  round,         // Floor, Ceiling, Round, Fraction, Sign, Step
  interpolation, // Lerp, Smoothstep, Remap
  range,         // Clamp, Saturate, Minimum, Maximum
  trigonometry,  // Sine, Cosine
  vector,        // Dot, Cross, Normalize, Length, Distance, Reflect, Fresnel Effect
  noise,         // Simple Noise, Voronoi
  channel,       // Swizzle, Split, Combine
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
    case shader_node_type::output_vertex:
    case shader_node_type::output_fragment_lit:
    case shader_node_type::output_fragment_unlit:
      return shader_node_category::output;
  }

  return shader_node_category::basic;
}

// Every node type but the three output sinks produces at least one output pin.
[[nodiscard]] inline auto shader_node_has_output(shader_node_type type) -> bool {
  return type != shader_node_type::output_vertex && type != shader_node_type::output_fragment_lit && type != shader_node_type::output_fragment_unlit;
}

// How many output pins a node has -- 1 for almost everything, 5 for Sample Texture (RGBA + R/G/B/A
// individually, matching Unity Shader Graph's own Sample Texture 2D node so a plain channel doesn't
// always need a Split node right after it), 4 for Split (R/G/B/A, always scalar -- a component
// beyond the input's own width just reads 0, not an error), 3 for Combine (RG/RGB/RGBA, each a
// different fixed width), 2 for Voronoi (Out, Cells), 0 for the three sinks. Shared by the editor's
// canvas (how many output pins to draw) and codegen (which output pin a downstream edge's from_pin
// actually reads).
[[nodiscard]] inline auto shader_node_output_count(shader_node_type type) -> std::size_t {
  if (!shader_node_has_output(type)) {
    return 0u;
  }

  if (type == shader_node_type::texture_sample) return 5u;
  if (type == shader_node_type::split) return 4u;
  if (type == shader_node_type::combine) return 3u;
  if (type == shader_node_type::voronoi) return 2u; // Out, Cells
  return 1u;
}

// Editor-only label for a node's Nth output pin -- purely descriptive, codegen doesn't consult
// this. Sample Texture's five are RGBA/R/G/B/A, Split's four are R/G/B/A, Combine's three are
// RG/RGB/RGBA; every other node has a single output, generically named "Out" (matching Unity Shader
// Graph's own convention for a node with nothing more specific to call it).
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

// Shared by shader_graph_codegen.cpp (how many input pins to read) and the editor's node canvas
// (how many input pins to draw) -- one source of truth for the node library's shape.
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
    case shader_node_type::constant_float:
    case shader_node_type::constant_vector2:
    case shader_node_type::constant_vector3:
    case shader_node_type::constant_vector4:
    case shader_node_type::constant_color:
      return 0u;
    case shader_node_type::texture_sample:
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
    case shader_node_type::simple_noise: // UV, Scale
      return 2u;
    case shader_node_type::lerp:
    case shader_node_type::smoothstep:
    case shader_node_type::clamp: // In, Min, Max
    case shader_node_type::remap: // In, In Min Max, Out Min Max
    case shader_node_type::fresnel_effect: // Normal, View Dir, Power
    case shader_node_type::voronoi: // UV, Angle Offset, Cell Density
    case shader_node_type::output_vertex: // Position, Normal, Tangent
    case shader_node_type::output_fragment_unlit: // Color, Alpha, Alpha Clip Threshold
      return 3u;
    case shader_node_type::combine: // R, G, B, A -- each optional, defaulting to 0
      return 4u;
    case shader_node_type::output_fragment_lit: // Albedo, Normal, Metallic, Roughness, Emission, Occlusion, Alpha, Alpha Clip Threshold
      return 8u;
  }

  return 0u;
}

// Editor-only label for a node's Nth input pin -- purely descriptive, codegen doesn't consult this.
[[nodiscard]] inline auto shader_node_input_label(shader_node_type type, std::size_t pin) -> const char* {
  switch (type) {
    case shader_node_type::texture_sample: return "UV";
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

// The last alternative (std::string) is Swizzle's pattern only -- up to 4 characters from
// {r,g,b,a}, one per output component, each naming which of the input's own components feeds it
// (e.g. "bgra" swaps red and blue; "rrr" splats red across a float3). Unused by every other type.
// math::vector4 (Vector4) is deliberately its own alternative from math::color (Color) even though
// both are 4 plain floats -- same reasoning Unity Shader Graph's own separate Vector4/Color nodes
// have: a Color's 4th component is conventionally alpha with color-picker UI/semantics, a Vector4's
// isn't necessarily either.
using shader_graph_node_value = std::variant<std::monostate, std::float_t, math::vector2, math::vector3, math::vector4, math::color, texture_handle, std::string>;

struct shader_graph_node {
  std::uint32_t id{0u};
  shader_node_type type{shader_node_type::constant_float};
  math::vector2 editor_position{0.0f, 0.0f};
  std::string name{};       // exposed-parameter / texture-slot display name
  bool exposed{false};      // constant_* only -- becomes a material-tunable generic_params slot
  bool preview{false};      // editor-only, like editor_position -- codegen never reads this; shows
                             // an inline 2D preview swatch on this node in the graph editor
  shader_graph_node_value value{};
}; // struct shader_graph_node

// A Swizzle node's stored pattern -- always exactly 4 characters from {r,g,b,a} once the editor's
// touched it, but this is the one place every reader (codegen, the node-inspector dropdowns, the
// canvas node's own inline label) falls back to the identity "rgba" for a hand-edited/legacy
// .shadergraph file's node.value, instead of each duplicating that same fallback separately.
[[nodiscard]] inline auto shader_node_swizzle_pattern(const shader_graph_node& node) -> std::string {
  const auto pattern = std::holds_alternative<std::string>(node.value) ? std::get<std::string>(node.value) : std::string{};
  return pattern.size() == 4u ? pattern : std::string{"rgba"};
}

// A Scene Depth node's stored sampling mode ("raw"/"eye"/"linear01") -- same fallback reasoning as
// shader_node_swizzle_pattern above, for a hand-edited/legacy file whose node.value isn't one of the
// three recognized strings. "linear01" (matches Unity Shader Graph's own default) is the fallback.
[[nodiscard]] inline auto shader_node_scene_depth_mode(const shader_graph_node& node) -> std::string {
  const auto mode = std::holds_alternative<std::string>(node.value) ? std::get<std::string>(node.value) : std::string{};
  return (mode == "raw" || mode == "eye" || mode == "linear01") ? mode : std::string{"linear01"};
}

struct shader_graph_edge {
  std::uint32_t from_node{0u};
  std::uint32_t from_pin{0u}; // index into the source node type's output list -- 0 except Split's/Combine's several
  std::uint32_t to_node{0u};
  std::uint32_t to_pin{0u};   // index into the target node type's fixed input list
}; // struct shader_graph_edge

// The type a node's Nth output pin has regardless of how it's wired -- nullopt means "dynamic",
// see shader_graph_type_resolver::output_type. Every node's own doc comment in shader_node_type
// lists its shape; this and shader_node_fixed_input_type are that shape's authoritative source.
// Only Combine varies by pin (RG/RGB/RGBA); every other fixed node answers the same regardless.
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
      return shader_value_type::vector3;
    case shader_node_type::constant_float:
    case shader_node_type::dot:
    case shader_node_type::split: // every Split output (R/G/B/A) is a scalar, regardless of pin
    case shader_node_type::time:
    case shader_node_type::delta_time:
    case shader_node_type::scene_depth:
    case shader_node_type::length:
    case shader_node_type::distance:
    case shader_node_type::fresnel_effect:
    case shader_node_type::simple_noise:
    case shader_node_type::voronoi: // Out and Cells are both scalar, regardless of pin
      return shader_value_type::scalar;
    case shader_node_type::constant_vector4:
    case shader_node_type::constant_color:
      return shader_value_type::vector4;
    case shader_node_type::texture_sample: // pin 0 (RGBA) is vector4; R/G/B/A (pins 1-4) are each scalar
      return pin == 0u ? shader_value_type::vector4 : shader_value_type::scalar;
    case shader_node_type::combine: // RG / RGB / RGBA
      return pin == 0u ? shader_value_type::vector2 : pin == 1u ? shader_value_type::vector3 : shader_value_type::vector4;
    default:
      // add/subtract/multiply/divide/lerp/normalize/saturate/swizzle/pow/step/smoothstep/negate/
      // one_minus/absolute/floor/ceiling/round/fraction/sign/minimum/maximum/clamp/remap -- dynamic
      return std::nullopt;
  }
}

// The type a node's input pin requires regardless of how it's wired -- nullopt means "dynamic"
// (matches the node's own resolved operating type once one exists, or accepts anything until then;
// see shader_graph_type_resolver::accepts).
[[nodiscard]] inline auto shader_node_fixed_input_type(shader_node_type type, std::size_t pin) -> std::optional<shader_value_type> {
  switch (type) {
    case shader_node_type::texture_sample:
    case shader_node_type::scene_depth:
      return shader_value_type::vector2; // uv
    case shader_node_type::cross:
    case shader_node_type::reflect:
      return shader_value_type::vector3; // both pins
    case shader_node_type::combine:
      return shader_value_type::scalar; // R/G/B/A, every pin
    case shader_node_type::remap:
      if (pin == 0u) return std::nullopt; // In -- dynamic
      return shader_value_type::vector2; // In Min Max / Out Min Max, each a matched (min,max) pair
    case shader_node_type::fresnel_effect:
      return pin == 2u ? shader_value_type::scalar : shader_value_type::vector3; // Normal/View Dir (vector3) / Power (scalar)
    case shader_node_type::simple_noise:
    case shader_node_type::voronoi:
      return pin == 0u ? shader_value_type::vector2 : shader_value_type::scalar; // UV (vector2) / everything else (scalar)
    case shader_node_type::output_vertex:
      return shader_value_type::vector3; // Position/Normal/Tangent
    case shader_node_type::output_fragment_unlit:
      return pin == 0u ? shader_value_type::vector3 : shader_value_type::scalar; // Color / Alpha / Alpha Clip Threshold
    case shader_node_type::output_fragment_lit:
      switch (pin) {
        case 0u: return shader_value_type::vector3; // Albedo
        case 1u: return shader_value_type::vector3; // Normal
        case 2u: return shader_value_type::scalar;  // Metallic
        case 3u: return shader_value_type::scalar;  // Roughness
        case 4u: return shader_value_type::vector3; // Emission
        case 5u: return shader_value_type::scalar;  // Occlusion
        default: return shader_value_type::scalar;  // Alpha / Alpha Clip Threshold
      }
    default:
      // add/subtract/multiply/divide/lerp/dot/normalize/saturate/swizzle/pow/step/smoothstep/split/
      // negate/one_minus/absolute/floor/ceiling/round/fraction/sign/minimum/maximum/clamp/length/
      // distance -- fully dynamic, no restriction (Split and Swizzle included -- Unity's own Split/
      // Swizzle nodes both accept "a vector of any dimension", scalar included).
      return std::nullopt;
  }
}

// Infers every dynamic node's actual current type from how the graph is wired, and validates
// prospective connections against it -- the single source of truth for both the editor's
// connect-time rejection and codegen's own pass over a possibly hand-edited/legacy graph. Same
// backward-DFS-with-memo shape as shader_graph_codegen_walker (shader_graph_codegen.cpp), just
// inferring types instead of emitting expressions.
class shader_graph_type_resolver {

public:

  shader_graph_type_resolver(const std::vector<shader_graph_node>& nodes, const std::vector<shader_graph_edge>& edges) {
    for (const auto& node : nodes) {
      _nodes_by_id.emplace(node.id, &node);
    }

    for (const auto& edge : edges) {
      _incoming.emplace(_key(edge.to_node, edge.to_pin), _key(edge.from_node, edge.from_pin));
    }
  }

  // The type (node_id, pin)'s output currently produces -- nullopt if that can't be determined yet
  // (nothing wired up to establish it) or the node has no output at all. Only Split's four pins and
  // Combine's three are cases where different pins of the same node answer differently; `pin`
  // defaults to 0 for every single-output node.
  [[nodiscard]] auto output_type(std::uint32_t node_id, std::uint32_t pin = 0u) -> std::optional<shader_value_type> {
    const auto key = _key(node_id, pin);

    if (const auto cached = _memo.find(key); cached != _memo.end()) {
      return cached->second;
    }

    _memo.emplace(key, std::nullopt); // break cycles: a (node, pin) re-entered mid-resolution reads as unresolved

    const auto result = _resolve(node_id, pin);
    _memo[key] = result;
    return result;
  }

  // The type (node_id, pin)'s input pin currently expects -- nullopt if it's dynamic and nothing
  // has wired up an operating type for the node yet. Editor-display only (the width shown next to
  // an input pin in the canvas): shader_node_fixed_input_type's answer where the pin is fixed,
  // otherwise the node's own resolved operating type (same value accepts() already falls back to
  // for a dynamic pin), which for a node's own dominant/only input pin (Split, Swizzle, Normalize,
  // ...) is exactly the width of whatever's plugged into it.
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

  // Does connecting a value of `source_type` into (node_id, pin) satisfy that pin's requirement?
  // Fixed pins require an exact match, with one deliberate exception: a float4 (Color, Sample
  // Texture, Combine's RGBA) always satisfies a float3-typed pin by dropping alpha -- as lossless/
  // expected a narrowing as the scalar broadcast below (every color has an obvious "drop alpha"
  // reading), so treated the same way; Split/Swizzle/Combine are still what you reach for beyond
  // that. Dynamic pins require an exact match (or that same float4->float3 exception) to the
  // node's own resolved operating type once one exists, or accept anything before one does, and
  // additionally always accept a plain scalar regardless (native, lossless broadcast).
  [[nodiscard]] auto accepts(std::uint32_t node_id, std::uint32_t pin, std::optional<shader_value_type> source_type) -> bool {
    const auto entry = _nodes_by_id.find(node_id);

    if (entry == _nodes_by_id.end() || !source_type) {
      return entry != _nodes_by_id.end(); // unresolved source -- nothing to validate against yet, allow
    }

    const auto type = entry->second->type;

    if (const auto fixed = shader_node_fixed_input_type(type, pin)) {
      return *fixed == *source_type || (*fixed == shader_value_type::vector3 && *source_type == shader_value_type::vector4);
    }

    if (*source_type == shader_value_type::scalar) {
      return true;
    }

    // Excludes `pin` itself from establishing the operating type: this validates what's allowed to
    // REPLACE whatever's currently connected to `pin`, so that stale connection shouldn't be what
    // the new candidate gets measured against (otherwise you could never independently rewire one
    // side of e.g. an Add node to a different width without first deleting its other side too).
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

    if (const auto fixed = shader_node_fixed_output_type(entry->second->type, pin)) {
      return fixed;
    }

    return _operating_type(node_id, std::nullopt);
  }

  // The width whichever "dominant" input pin(s) establish -- add/subtract/multiply/divide/lerp/
  // normalize/saturate/swizzle/pow/step/smoothstep's output IS this width (used by _resolve), and
  // Dot's two inputs must dynamically match at this width too even though its own output is fixed
  // scalar (used directly by accepts(), bypassing _resolve() for exactly that reason). Pins the
  // dominant-pins list excludes are always allowed to be a plain scalar regardless (Lerp's T,
  // Pow's exponent, Step/Smoothstep's x). `exclude_pin`, when set, skips that one pin entirely --
  // see accepts()'s own doc comment for why. A non-scalar dominant pin wins immediately.
  //
  // A scalar dominant pin is only ever a real, node-wide "operating type" once EVERY dominant pin
  // has actually been examined (none excluded, none simply unconnected) and every one of them
  // resolved scalar -- a scalar never constrains anything on its own (it broadcasts against any
  // width), so a single-dominant-pin node (Pow, Normalize, ...) connected scalar genuinely IS
  // scalar (its one pin *is* every dominant pin), but a two-dominant-pin node (Add, Multiply, Dot,
  // ...) with only ONE side connected scalar is NOT yet scalar-locked: the still-unconnected or
  // currently-being-replaced other side could still turn out non-scalar, and the scalar side would
  // just broadcast against it, exactly as it does when the connection order is reversed. Getting
  // this wrong is exactly why multiply(float, float4) used to be rejected while
  // multiply(float4, float) (float4 connected first, establishing the type outright before the
  // scalar side that never contests it) was accepted -- both must be equally valid.
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

  // Split and Swizzle each have exactly one dominant pin (their own single input) purely so
  // _operating_type can resolve THEIR output width from it (Swizzle's output IS its input's width;
  // Split's is separately fixed-scalar-per-pin, so this list is never actually consulted for its
  // own output -- only by accepts() validating a replacement connection into that same pin, where
  // excluding it via exclude_pin correctly leaves nothing to compare against, i.e. "accepts
  // anything", matching Unity's own Split/Swizzle behavior).
  [[nodiscard]] static auto _dominant_pins(shader_node_type type) -> std::initializer_list<std::uint32_t> {
    switch (type) {
      case shader_node_type::add:
      case shader_node_type::subtract:
      case shader_node_type::multiply:
      case shader_node_type::divide:
      case shader_node_type::step:
      case shader_node_type::lerp:       // T (pin 2) never drives the type
      case shader_node_type::smoothstep: // x (pin 2) never drives the type
      case shader_node_type::dot:        // fixed scalar output, but its 2 inputs must still dynamically match each other
      case shader_node_type::minimum:
      case shader_node_type::maximum:
      case shader_node_type::distance:   // fixed scalar output, same reasoning as Dot
        return {0u, 1u};
      case shader_node_type::pow:        // exponent (pin 1) never drives the type
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
      case shader_node_type::clamp:  // Min/Max (pins 1,2) never drive the type
      case shader_node_type::length: // fixed scalar output, but In's own width still needs to be displayable/re-checkable
      case shader_node_type::remap:  // In Min Max/Out Min Max (pins 1,2) never drive the type
        return {0u};
      default:
        return {};
    }
  }

  std::unordered_map<std::uint32_t, const shader_graph_node*> _nodes_by_id{};
  std::unordered_map<std::uint64_t, std::uint64_t> _incoming{}; // (to_node<<32|to_pin) -> (from_node<<32|from_pin)
  std::unordered_map<std::uint64_t, std::optional<shader_value_type>> _memo{}; // (node<<32|pin) -> type

}; // class shader_graph_type_resolver

enum class shader_graph_parameter_type : std::uint8_t {
  float_value,
  vector2_value,
  vector3_value,
  vector4_value,
  color_value,
  texture_value
}; // enum class shader_graph_parameter_type

// Derived (not stored): one entry per exposed constant_* node plus one per texture_sample node, in
// ascending node-id order. A material referencing this graph stores one value per parameter here,
// in the same order -- see shader_graph::parameters() and material::create_info::generic_params/
// generic_textures.
struct shader_graph_parameter {
  std::uint32_t node_id{0u};
  std::string name{};
  shader_graph_parameter_type type{shader_graph_parameter_type::float_value};
  std::uint32_t slot{0u}; // index into generic_params[] (float/vector2/vector3/vector4/color) or generic_textures[]
}; // struct shader_graph_parameter

// Free function (not a shader_graph method) so codegen can call it directly against a
// create_info/node list before any shader_graph asset object exists -- shader_graph::parameters()
// below is just this, applied to an already-loaded graph's own nodes.
[[nodiscard]] inline auto compute_shader_graph_parameters(const std::vector<shader_graph_node>& nodes) -> std::vector<shader_graph_parameter> {
  auto ordered = nodes;

  std::ranges::sort(ordered, {}, &shader_graph_node::id);

  auto result = std::vector<shader_graph_parameter>{};
  auto float_slot = std::uint32_t{0u};
  auto texture_slot = std::uint32_t{0u};

  for (const auto& node : ordered) {
    if (node.type == shader_node_type::texture_sample) {
      result.push_back(shader_graph_parameter{node.id, node.name, shader_graph_parameter_type::texture_value, texture_slot++});
      continue;
    }

    if (!node.exposed) {
      continue;
    }

    switch (node.type) {
      case shader_node_type::constant_float:
        result.push_back(shader_graph_parameter{node.id, node.name, shader_graph_parameter_type::float_value, float_slot++});
        break;
      case shader_node_type::constant_vector2:
        result.push_back(shader_graph_parameter{node.id, node.name, shader_graph_parameter_type::vector2_value, float_slot++});
        break;
      case shader_node_type::constant_vector3:
        result.push_back(shader_graph_parameter{node.id, node.name, shader_graph_parameter_type::vector3_value, float_slot++});
        break;
      case shader_node_type::constant_vector4:
        result.push_back(shader_graph_parameter{node.id, node.name, shader_graph_parameter_type::vector4_value, float_slot++});
        break;
      case shader_node_type::constant_color:
        result.push_back(shader_graph_parameter{node.id, node.name, shader_graph_parameter_type::color_value, float_slot++});
        break;
      default:
        break; // exposed is only meaningful on constant_* -- ignored elsewhere
    }
  }

  return result;
}

class shader_graph final : public loadable {

  friend class asset_residency;

public:

  struct create_info {
    std::string name{"shader_graph"};
    std::vector<shader_graph_node> nodes{};
    std::vector<shader_graph_edge> edges{};
  }; // struct create_info

  shader_graph() = default;

  explicit shader_graph(const create_info& create_info)
  : _name{create_info.name},
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
    return compute_shader_graph_parameters(_nodes);
  }

private:

  std::string _name{"shader_graph"};
  std::vector<shader_graph_node> _nodes{};
  std::vector<shader_graph_edge> _edges{};
  math::uuid _id{math::uuid::nil()};

}; // class shader_graph

using shader_graph_handle = asset_handle<shader_graph>;

// A fresh material's own generic_params/generic_textures (material.hpp) start zero-initialized --
// nothing else copies a graph's own authored node defaults (the same values shown in the graph
// editor's node inspector and used for its 2D/3D preview) into them. Without this, a material that
// exposes e.g. a white "Albedo" tint silently starts multiplying everything by black until someone
// manually sets every exposed row in the Material Inspector -- exactly what looks like a broken/
// black shader graph material until you know to look there. Called once when a material's shader
// graph is (re)assigned (see inspector_asset_editors.cpp's draw_shader_graph_picker call site) --
// not on every frame, so a value the user already edited in the Material Inspector is never
// silently overwritten by this.
[[nodiscard]] inline auto shader_graph_default_generic_params(const shader_graph& graph) -> std::array<math::vector4, shader_graph_max_params> {
  auto result = std::array<math::vector4, shader_graph_max_params>{};

  for (const auto& parameter : graph.parameters()) {
    if (parameter.type == shader_graph_parameter_type::texture_value || parameter.slot >= result.size()) {
      continue;
    }

    const auto node = std::ranges::find(graph.nodes(), parameter.node_id, &shader_graph_node::id);

    if (node == graph.nodes().end()) {
      continue;
    }

    result[parameter.slot] = std::visit([]<typename T>(const T& value) -> math::vector4 {
      if constexpr (std::is_same_v<T, std::float_t>) return math::vector4{value, 0.0f, 0.0f, 0.0f};
      else if constexpr (std::is_same_v<T, math::vector2>) return math::vector4{value.x(), value.y(), 0.0f, 0.0f};
      else if constexpr (std::is_same_v<T, math::vector3>) return math::vector4{value.x(), value.y(), value.z(), 0.0f};
      else if constexpr (std::is_same_v<T, math::vector4>) return value;
      else if constexpr (std::is_same_v<T, math::color>) return math::vector4{value.r(), value.g(), value.b(), value.a()};
      else return math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
    }, node->value);
  }

  return result;
}

// Same reasoning as shader_graph_default_generic_params, for exposed Sample Texture nodes -- each
// one's own node.value (its "Default Texture" picker, see shader_graph_panel.cpp) becomes the
// material's initial slot instead of an unset/white fallback.
[[nodiscard]] inline auto shader_graph_default_generic_textures(const shader_graph& graph) -> std::array<texture_handle, shader_graph_max_textures> {
  auto result = std::array<texture_handle, shader_graph_max_textures>{};

  for (const auto& parameter : graph.parameters()) {
    if (parameter.type != shader_graph_parameter_type::texture_value || parameter.slot >= result.size()) {
      continue;
    }

    const auto node = std::ranges::find(graph.nodes(), parameter.node_id, &shader_graph_node::id);

    if (node == graph.nodes().end() || !std::holds_alternative<texture_handle>(node->value)) {
      continue;
    }

    result[parameter.slot] = std::get<texture_handle>(node->value);
  }

  return result;
}

// The generated Slang type/module name for a graph, and its virtual shader_cache path -- shared by
// asset_cooker::cook_shader_graph (writes the file there) and every render pass that needs to
// compile/bind a graph-driven material's pipeline (looks it up there), so the convention can't
// drift between the two.
//
// `generation` (shader_graph::generation(), bumped by asset_residency::update_shader_graph -- see
// loadable.hpp -- which only runs on Save and on the initial async load, not on every editor
// keystroke; see shader_graph_panel's own doc comment for why) is folded into both so a saved
// edit's path never collides with the previous one: shader_cache/pipeline_cache are plain
// path-keyed maps with no invalidation/re-check of their own (see their own doc comments), so
// re-cooking the SAME path after an edit would just keep serving the stale already-compiled shader
// until the app restarted. A generation-suffixed path instead makes every Save a cache MISS, so it
// always gets freshly compiled -- at the cost of the previous generation's now-unreachable
// shader/pipeline objects just sitting in those caches unused for the rest of the run, rather than
// being torn down (tearing down a GPU shader/pipeline that a previous frame's still-in-flight
// command buffer might reference needs the same frame-retirement machinery resource_registry
// already has for buffers/images -- shader_cache/pipeline_cache don't have it, and adding it is
// real scope beyond "make saved edits apply").
// ponytail: this leaks one shader+pipeline object per Save for the process's lifetime -- fine for
// an editor session iterating on a handful of graphs, not fine if that ever becomes hundreds of
// saves without a restart. Upgrade path: give shader_cache/pipeline_cache real eviction with
// frame-safe deferred destruction (mirroring resource_registry's retirement), then drop the
// generation suffix and evict-and-recompile the stable path in place instead.
[[nodiscard]] inline auto shader_graph_generated_name(const math::uuid& id, std::uint64_t generation) -> std::string {
  return fmt::format("shader_graph_{}_{}", id.value(), generation);
}

[[nodiscard]] inline auto shader_graph_generated_path(const math::uuid& id, std::uint64_t generation) -> std::string {
  return fmt::format("engine://shaders/generated/{}.slang", shader_graph_generated_name(id, generation));
}

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SHADER_GRAPH_HPP_
