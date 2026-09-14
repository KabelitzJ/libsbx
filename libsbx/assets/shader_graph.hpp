// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_SHADER_GRAPH_HPP_
#define LIBSBX_ASSETS_SHADER_GRAPH_HPP_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <fmt/format.h>

#include <libsbx/math/color.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>

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

// The engine's only built-in shading models are pbr/unlit (material::shading_model) -- everything
// else, a user composes as a shader_graph material. This is the node library a graph can be built
// from; see shader_graph_codegen.hpp for how each of these turns into Slang.
enum class shader_node_type : std::uint8_t {
  input_uv,
  input_normal,
  input_view_dir,
  input_light_dir,   // valid only when reachable from output_direct -- direct() has `l`, extra() doesn't
  input_light_color, // same restriction -- direct() has `radiance`, extra() doesn't
  constant_float,
  constant_vector3,
  constant_color,
  texture_sample,     // 1 input (uv, optional -- falls back to surface.uv if unconnected)
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
  output_direct, // sink: this graph's direct()-lighting result. Exactly one required per graph.
  output_extra   // sink: this graph's extra() (view-dependent, e.g. rim) result. Optional.
}; // enum class shader_node_type

// Canonical string form, shared by YAML parse (asset_cooker_shader_graph.cpp), YAML save
// (asset_residency.cpp), and codegen's error messages (shader_graph_codegen.cpp) -- one mapping
// table instead of three hand-duplicated switches.
[[nodiscard]] inline auto shader_node_type_to_string(shader_node_type type) -> const char* {
  switch (type) {
    case shader_node_type::input_uv: return "input_uv";
    case shader_node_type::input_normal: return "input_normal";
    case shader_node_type::input_view_dir: return "input_view_dir";
    case shader_node_type::input_light_dir: return "input_light_dir";
    case shader_node_type::input_light_color: return "input_light_color";
    case shader_node_type::constant_float: return "constant_float";
    case shader_node_type::constant_vector3: return "constant_vector3";
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
    case shader_node_type::output_direct: return "output_direct";
    case shader_node_type::output_extra: return "output_extra";
  }

  return "constant_float";
}

[[nodiscard]] inline auto shader_node_type_from_string(std::string_view value) -> shader_node_type {
  if (value == "input_uv") return shader_node_type::input_uv;
  if (value == "input_normal") return shader_node_type::input_normal;
  if (value == "input_view_dir") return shader_node_type::input_view_dir;
  if (value == "input_light_dir") return shader_node_type::input_light_dir;
  if (value == "input_light_color") return shader_node_type::input_light_color;
  if (value == "constant_float") return shader_node_type::constant_float;
  if (value == "constant_vector3") return shader_node_type::constant_vector3;
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
  if (value == "output_direct") return shader_node_type::output_direct;
  if (value == "output_extra") return shader_node_type::output_extra;
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
    case shader_node_type::input_light_dir: return "Light Direction";
    case shader_node_type::input_light_color: return "Light Color";
    case shader_node_type::constant_float: return "Float";
    case shader_node_type::constant_vector3: return "Vector3";
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
    case shader_node_type::output_direct: return "Direct Output";
    case shader_node_type::output_extra: return "Extra Output";
  }

  return "?";
}

// Node palette grouping, for the editor's "Add Node" menu -- purely a UI concern, no effect on
// codegen or serialization.
enum class shader_node_category : std::uint8_t {
  input,
  constant,
  texture,
  math,
  output
}; // enum class shader_node_category

[[nodiscard]] inline auto shader_node_category_of(shader_node_type type) -> shader_node_category {
  switch (type) {
    case shader_node_type::input_uv:
    case shader_node_type::input_normal:
    case shader_node_type::input_view_dir:
    case shader_node_type::input_light_dir:
    case shader_node_type::input_light_color:
      return shader_node_category::input;
    case shader_node_type::constant_float:
    case shader_node_type::constant_vector3:
    case shader_node_type::constant_color:
      return shader_node_category::constant;
    case shader_node_type::texture_sample:
      return shader_node_category::texture;
    case shader_node_type::add:
    case shader_node_type::subtract:
    case shader_node_type::multiply:
    case shader_node_type::divide:
    case shader_node_type::lerp:
    case shader_node_type::dot:
    case shader_node_type::cross:
    case shader_node_type::normalize:
    case shader_node_type::saturate:
    case shader_node_type::pow:
    case shader_node_type::step:
    case shader_node_type::smoothstep:
      return shader_node_category::math;
    case shader_node_type::output_direct:
    case shader_node_type::output_extra:
      return shader_node_category::output;
  }

  return shader_node_category::math;
}

// Every node type but the two output sinks has exactly one (untyped -- Slang's own implicit
// scalar<->vector broadcast covers mismatches) output pin.
[[nodiscard]] inline auto shader_node_has_output(shader_node_type type) -> bool {
  return type != shader_node_type::output_direct && type != shader_node_type::output_extra;
}

// Shared by shader_graph_codegen.cpp (how many input pins to read) and the editor's node canvas
// (how many input pins to draw) -- one source of truth for the node library's shape.
[[nodiscard]] inline auto shader_node_input_count(shader_node_type type) -> std::size_t {
  switch (type) {
    case shader_node_type::input_uv:
    case shader_node_type::input_normal:
    case shader_node_type::input_view_dir:
    case shader_node_type::input_light_dir:
    case shader_node_type::input_light_color:
    case shader_node_type::constant_float:
    case shader_node_type::constant_vector3:
    case shader_node_type::constant_color:
      return 0u;
    case shader_node_type::texture_sample:
    case shader_node_type::normalize:
    case shader_node_type::saturate:
    case shader_node_type::output_direct:
    case shader_node_type::output_extra:
      return 1u;
    case shader_node_type::add:
    case shader_node_type::subtract:
    case shader_node_type::multiply:
    case shader_node_type::divide:
    case shader_node_type::dot:
    case shader_node_type::cross:
    case shader_node_type::pow:
    case shader_node_type::step:
      return 2u;
    case shader_node_type::lerp:
    case shader_node_type::smoothstep:
      return 3u;
  }

  return 0u;
}

// Editor-only label for a node's Nth input pin -- purely descriptive, codegen doesn't consult this.
[[nodiscard]] inline auto shader_node_input_label(shader_node_type type, std::size_t pin) -> const char* {
  switch (type) {
    case shader_node_type::texture_sample: return "UV";
    case shader_node_type::normalize: return "In";
    case shader_node_type::saturate: return "In";
    case shader_node_type::output_direct: return "Color";
    case shader_node_type::output_extra: return "Color";
    case shader_node_type::add:
    case shader_node_type::subtract:
    case shader_node_type::multiply:
    case shader_node_type::divide:
    case shader_node_type::dot:
    case shader_node_type::cross:
    case shader_node_type::pow:
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
    default:
      return "In";
  }
}

using shader_graph_node_value = std::variant<std::monostate, std::float_t, math::vector3, math::color, texture_handle>;

struct shader_graph_node {
  std::uint32_t id{0u};
  shader_node_type type{shader_node_type::constant_float};
  math::vector2 editor_position{0.0f, 0.0f};
  std::string name{};       // exposed-parameter / texture-slot display name
  bool exposed{false};      // constant_* only -- becomes a material-tunable generic_params slot
  shader_graph_node_value value{};
}; // struct shader_graph_node

struct shader_graph_edge {
  std::uint32_t from_node{0u};
  std::uint32_t from_pin{0u}; // always 0 today -- every node type has exactly one output
  std::uint32_t to_node{0u};
  std::uint32_t to_pin{0u};   // index into the target node type's fixed input list
}; // struct shader_graph_edge

enum class shader_graph_parameter_type : std::uint8_t {
  float_value,
  vector3_value,
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
  std::uint32_t slot{0u}; // index into generic_params[] (float/vector3/color) or generic_textures[]
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
      case shader_node_type::constant_vector3:
        result.push_back(shader_graph_parameter{node.id, node.name, shader_graph_parameter_type::vector3_value, float_slot++});
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

// The generated Slang type/module name for a graph, and its virtual shader_cache path -- shared by
// asset_cooker::cook_shader_graph (writes the file there) and every render pass that needs to
// compile/bind a graph-driven material's pipeline (looks it up there), so the convention can't
// drift between the two.
[[nodiscard]] inline auto shader_graph_generated_name(const math::uuid& id) -> std::string {
  return fmt::format("shader_graph_{}", id.value());
}

[[nodiscard]] inline auto shader_graph_generated_path(const math::uuid& id) -> std::string {
  return fmt::format("engine://shaders/generated/{}.slang", shader_graph_generated_name(id));
}

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SHADER_GRAPH_HPP_
