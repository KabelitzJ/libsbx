// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/shader_graph_codegen.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>

namespace sbx::assets {

static auto is_direct_only_input(shader_node_type type) -> bool {
  return type == shader_node_type::input_light_dir || type == shader_node_type::input_light_color;
}

static auto component_count(const std::string& type) -> int {
  if (type == "float2") return 2;
  if (type == "float3") return 3;
  if (type == "float4") return 4;
  return 1; // "float", or anything unrecognized
}

// Slang only broadcasts a true scalar against a vector (float * float3 -> float3); two vectors of
// *different* non-scalar sizes (float3 * float4, e.g. a lighting term times a sampled texture's rgba)
// don't combine at all -- there's no implicit rule for that, it's a hard type error. So a binary
// math node's result type is the narrower of its two non-scalar operands, and whichever operand is
// wider than that gets explicitly truncated (via swizzle) before the operator is emitted; a scalar
// operand needs no truncation since it broadcasts natively either way.
static auto binary_result_type(const std::string& a, const std::string& b) -> const std::string& {
  const auto ca = component_count(a);
  const auto cb = component_count(b);
  if (ca == 1) return b; // scalar op vector -> vector
  if (cb == 1) return a; // vector op scalar -> vector
  return ca <= cb ? a : b; // two differently-sized vectors -- narrower wins, wider truncates to match
}

static auto coerce_operand(const std::string& expression, const std::string& type, const std::string& result_type) -> std::string {
  const auto from = component_count(type);
  const auto to = component_count(result_type);

  if (from <= to) {
    return expression; // scalar (broadcasts natively), or already the right size
  }

  static constexpr auto swizzles = std::array<const char*, 4u>{"x", "xy", "xyz", "xyzw"};

  return fmt::format("{}.{}", expression, swizzles[to - 1]);
}

struct emitted_value {
  std::string expression;
  std::string type;
}; // struct emitted_value

struct edge_key {
  std::uint32_t to_node;
  std::uint32_t to_pin;

  auto operator==(const edge_key&) const -> bool = default;
}; // struct edge_key

struct edge_key_hash {
  auto operator()(const edge_key& key) const -> std::size_t {
    return (static_cast<std::size_t>(key.to_node) << 32u) ^ static_cast<std::size_t>(key.to_pin);
  }
}; // struct edge_key_hash

// Walks a graph backward from one sink (output_direct's or output_extra's single input), emitting
// one Slang local per reachable node in dependency order (a post-order DFS -- a node's dependencies
// are always visited, and so emitted, before the node itself), and building the sink function's
// body as it goes. Doubles as reachability (nodes never visited are never emitted -- dead-code
// elimination as a side effect of the walk, not a separate pass) and cycle detection (a node
// re-entered while still on the visiting stack is a cycle).
class shader_graph_codegen_walker {

public:

  shader_graph_codegen_walker(const shader_graph::create_info& graph, bool is_extra_context)
  : _is_extra_context{is_extra_context} {
    for (const auto& node : graph.nodes) {
      _nodes_by_id.emplace(node.id, &node);
    }

    for (const auto& edge : graph.edges) {
      _incoming.emplace(edge_key{edge.to_node, edge.to_pin}, edge.from_node);
    }

    for (const auto& parameter : compute_shader_graph_parameters(graph.nodes)) {
      if (parameter.type == shader_graph_parameter_type::texture_value) {
        _texture_slot_of.emplace(parameter.node_id, parameter.slot);
      } else {
        _float_slot_of.emplace(parameter.node_id, parameter.slot);
      }
    }
  }

  // Returns the root's local variable name (or literal expression for a directly-inlined single
  // node), or nullopt if `error()` is set -- check that after every call.
  [[nodiscard]] auto visit_root(std::uint32_t sink_node_id) -> std::optional<emitted_value> {
    const auto entry = _incoming.find(edge_key{sink_node_id, 0u});

    if (entry == _incoming.end()) {
      _error = fmt::format("{} node {} has no input connected", _is_extra_context ? "output_extra" : "output_direct", sink_node_id);
      return std::nullopt;
    }

    return _visit(entry->second);
  }

  [[nodiscard]] auto error() const -> const std::string& {
    return _error;
  }

  [[nodiscard]] auto body() const -> const std::string& {
    return _body;
  }

private:

  [[nodiscard]] auto _visit(std::uint32_t node_id) -> std::optional<emitted_value> {
    if (!_error.empty()) {
      return std::nullopt;
    }

    if (const auto cached = _emitted.find(node_id); cached != _emitted.end()) {
      return cached->second;
    }

    if (_visiting.contains(node_id)) {
      _error = fmt::format("cycle detected at node {}", node_id);
      return std::nullopt;
    }

    const auto entry = _nodes_by_id.find(node_id);

    if (entry == _nodes_by_id.end()) {
      _error = fmt::format("edge references unknown node {}", node_id);
      return std::nullopt;
    }

    const auto& node = *entry->second;

    if (_is_extra_context && is_direct_only_input(node.type)) {
      _error = fmt::format("node {} ({}) is only valid feeding output_direct -- extra() has no light to read", node_id, shader_node_type_to_string(node.type));
      return std::nullopt;
    }

    _visiting.insert(node_id);

    const auto input_count = shader_node_input_count(node.type);
    auto inputs = std::vector<emitted_value>{};
    inputs.reserve(input_count);

    for (auto pin = std::uint32_t{0u}; pin < input_count; ++pin) {
      const auto source = _incoming.find(edge_key{node_id, pin});

      if (source == _incoming.end()) {
        // texture_sample's uv input (pin 0) is the one input allowed to be unconnected -- it falls
        // back to surface.uv, matching Unity Shader Graph's own Sample Texture 2D node convention.
        if (node.type == shader_node_type::texture_sample && pin == 0u) {
          inputs.push_back(emitted_value{"", "float2"});
          continue;
        }

        _error = fmt::format("node {} ({}) input {} is not connected", node_id, shader_node_type_to_string(node.type), pin);
        return std::nullopt;
      }

      const auto value = _visit(source->second);

      if (!value) {
        return std::nullopt;
      }

      inputs.push_back(*value);
    }

    _visiting.erase(node_id);

    const auto result = _emit(node, inputs);

    if (!_error.empty()) {
      return std::nullopt;
    }

    _emitted.emplace(node_id, result);

    return result;
  }

  [[nodiscard]] auto _emit(const shader_graph_node& node, const std::vector<emitted_value>& inputs) -> emitted_value {
    switch (node.type) {
      case shader_node_type::input_uv: return {"s.uv", "float2"};
      case shader_node_type::input_normal: return {"s.n", "float3"};
      case shader_node_type::input_view_dir: return {"s.v", "float3"};
      case shader_node_type::input_light_dir: return {"l", "float3"};
      case shader_node_type::input_light_color: return {"radiance", "float3"};

      case shader_node_type::constant_float: {
        if (node.exposed) {
          return {fmt::format("s.generic_params[{}].x", _float_slot_of.at(node.id)), "float"};
        }
        const auto value = std::holds_alternative<std::float_t>(node.value) ? std::get<std::float_t>(node.value) : 0.0f;
        return {fmt::format("float({})", value), "float"};
      }

      case shader_node_type::constant_vector3: {
        if (node.exposed) {
          return {fmt::format("s.generic_params[{}].xyz", _float_slot_of.at(node.id)), "float3"};
        }
        const auto value = std::holds_alternative<math::vector3>(node.value) ? std::get<math::vector3>(node.value) : math::vector3{};
        return {fmt::format("float3({}, {}, {})", value.x(), value.y(), value.z()), "float3"};
      }

      case shader_node_type::constant_color: {
        if (node.exposed) {
          return {fmt::format("s.generic_params[{}]", _float_slot_of.at(node.id)), "float4"};
        }
        const auto value = std::holds_alternative<math::color>(node.value) ? std::get<math::color>(node.value) : math::color{};
        return {fmt::format("float4({}, {}, {}, {})", value.r(), value.g(), value.b(), value.a()), "float4"};
      }

      case shader_node_type::texture_sample: {
        // ponytail: node.value (the node's own "default" texture, settable in the graph editor's
        // node inspector -- see shader_graph_panel.cpp) is never read here -- an unset material
        // slot always samples asset_residency's plain white fallback (same as every other texture
        // slot), not this node's default. Wiring a real per-graph default through would mean
        // embedding it as a second bindless index codegen could fall back to at runtime; revisit
        // if a graph author actually needs one.
        const auto slot = _texture_slot_of.at(node.id);
        const auto& uv = inputs[0].expression.empty() ? std::string{"s.uv"} : inputs[0].expression;
        return {fmt::format("textures[s.generic_textures[{}]].Sample(samplers[s.sampler_index], {})", slot, uv), "float4"};
      }

      case shader_node_type::add:
      case shader_node_type::subtract:
      case shader_node_type::multiply:
      case shader_node_type::divide: {
        const auto& result_type = binary_result_type(inputs[0].type, inputs[1].type);
        const auto a = coerce_operand(inputs[0].expression, inputs[0].type, result_type);
        const auto b = coerce_operand(inputs[1].expression, inputs[1].type, result_type);

        const auto* op = node.type == shader_node_type::add ? "+" : node.type == shader_node_type::subtract ? "-" : node.type == shader_node_type::multiply ? "*" : "/";

        return {fmt::format("({} {} {})", a, op, b), result_type};
      }

      case shader_node_type::lerp: {
        const auto& result_type = binary_result_type(inputs[0].type, inputs[1].type);
        const auto a = coerce_operand(inputs[0].expression, inputs[0].type, result_type);
        const auto b = coerce_operand(inputs[1].expression, inputs[1].type, result_type);
        const auto t = coerce_operand(inputs[2].expression, inputs[2].type, result_type);

        return {fmt::format("lerp({}, {}, {})", a, b, t), result_type};
      }
      case shader_node_type::dot: return {fmt::format("dot({}, {})", inputs[0].expression, inputs[1].expression), "float"};
      case shader_node_type::cross: return {fmt::format("cross({}, {})", inputs[0].expression, inputs[1].expression), "float3"};
      case shader_node_type::normalize: return {fmt::format("normalize({})", inputs[0].expression), inputs[0].type};
      case shader_node_type::saturate: return {fmt::format("saturate({})", inputs[0].expression), inputs[0].type};

      case shader_node_type::pow: {
        const auto& result_type = binary_result_type(inputs[0].type, inputs[1].type);
        const auto a = coerce_operand(inputs[0].expression, inputs[0].type, result_type);
        const auto b = coerce_operand(inputs[1].expression, inputs[1].type, result_type);

        return {fmt::format("pow({}, {})", a, b), result_type};
      }

      case shader_node_type::step: {
        const auto& result_type = binary_result_type(inputs[0].type, inputs[1].type);
        const auto edge = coerce_operand(inputs[0].expression, inputs[0].type, result_type);
        const auto x = coerce_operand(inputs[1].expression, inputs[1].type, result_type);

        return {fmt::format("step({}, {})", edge, x), result_type};
      }

      case shader_node_type::smoothstep: {
        const auto& edges_type = binary_result_type(inputs[0].type, inputs[1].type);
        const auto& result_type = binary_result_type(edges_type, inputs[2].type);
        const auto edge0 = coerce_operand(inputs[0].expression, inputs[0].type, result_type);
        const auto edge1 = coerce_operand(inputs[1].expression, inputs[1].type, result_type);
        const auto x = coerce_operand(inputs[2].expression, inputs[2].type, result_type);

        return {fmt::format("smoothstep({}, {}, {})", edge0, edge1, x), result_type};
      }

      case shader_node_type::output_direct:
      case shader_node_type::output_extra:
        return inputs[0]; // sinks are never reached here -- visit_root() reads their input directly
    }

    return {"float3(0.0, 0.0, 0.0)", "float3"};
  }

  bool _is_extra_context;
  std::unordered_map<std::uint32_t, const shader_graph_node*> _nodes_by_id{};
  std::unordered_map<edge_key, std::uint32_t, edge_key_hash> _incoming{};
  std::unordered_map<std::uint32_t, std::uint32_t> _float_slot_of{};
  std::unordered_map<std::uint32_t, std::uint32_t> _texture_slot_of{};
  std::unordered_map<std::uint32_t, emitted_value> _emitted{};
  std::unordered_set<std::uint32_t> _visiting{};
  std::string _body{};
  std::string _error{};

}; // class shader_graph_codegen_walker

// direct()/extra() both return float3 (surface color), but nothing constrains what type actually
// reaches an output_direct/output_extra node's input pin -- a bare Constant Color (float4) or a
// scalar math result feeding it directly is exactly as valid a graph as one that happens to end on
// a float3, and Slang has no implicit narrowing/widening for a return statement the way some other
// shading languages do. Coerce explicitly based on the root's actual type instead of asking every
// graph author to insert their own conversion node.
static auto coerce_to_float3(const std::string& expression, const std::string& type) -> std::string {
  if (type == "float4") return fmt::format("{}.rgb", expression); // drop alpha -- not meaningful for a lighting_model's color result
  if (type == "float2") return fmt::format("float3({}, 0.0)", expression);
  if (type == "float3") return expression;
  return fmt::format("float3({}, {}, {})", expression, expression, expression); // "float" -- splat to grayscale
}

static auto emit_function(const shader_graph::create_info& graph, std::uint32_t sink_id, std::string_view signature, bool is_extra_context, std::string& out_error) -> std::string {
  auto walker = shader_graph_codegen_walker{graph, is_extra_context};

  const auto root = walker.visit_root(sink_id);

  if (!walker.error().empty()) {
    out_error = walker.error();
    return {};
  }

  auto body = std::string{};

  body += walker.body();
  body += fmt::format("    return {};\n", coerce_to_float3(root->expression, root->type));

  return fmt::format("  static {} {{\n{}  }}\n", signature, body);
}

auto generate_shader_graph_source(const std::string& graph_name, const shader_graph::create_info& graph) -> shader_graph_codegen_result {
  const auto has_output_direct = std::ranges::any_of(graph.nodes, [](const shader_graph_node& node) { return node.type == shader_node_type::output_direct; });

  if (!has_output_direct) {
    return shader_graph_codegen_result{false, {}, "graph has no output_direct node"};
  }

  const auto output_direct_id = std::ranges::find_if(graph.nodes, [](const shader_graph_node& node) { return node.type == shader_node_type::output_direct; })->id;

  auto error = std::string{};
  const auto direct_body = emit_function(graph, output_direct_id, "float3 direct(surface s, float3 l, float3 radiance)", false, error);

  if (!error.empty()) {
    return shader_graph_codegen_result{false, {}, fmt::format("direct(): {}", error)};
  }

  auto extra_body = std::string{};

  if (const auto output_extra = std::ranges::find_if(graph.nodes, [](const shader_graph_node& node) { return node.type == shader_node_type::output_extra; }); output_extra != graph.nodes.end()) {
    extra_body = emit_function(graph, output_extra->id, "float3 extra(surface s)", true, error);

    if (!error.empty()) {
      return shader_graph_codegen_result{false, {}, fmt::format("extra(): {}", error)};
    }
  } else {
    extra_body = "  static float3 extra(surface s) {\n    return float3(0.0, 0.0, 0.0);\n  }\n";
  }

  auto source = fmt::format(
    "// AUTO-GENERATED by shader graph codegen. Do not hand-edit -- edit the .shadergraph source and re-cook.\n"
    // Root-relative, not bare -- the generated file lives in shaders/generated/, whose own search
    // path doesn't cover shaders/pbr/ where geometry_common.slang actually is; the shaders/ root
    // itself is always a search path (shader_compiler.cpp's _shaders_root), so this resolves
    // regardless of which subdirectory the generated file ends up in.
    "#include <pbr/geometry_common.slang>\n"
    "\n"
    "struct {0}_lighting_model : lighting_model {{\n"
    "{1}"
    "\n"
    "{2}"
    "}}; // struct {0}_lighting_model\n"
    "\n"
    "[shader(\"fragment\")]\n"
    "Policy::Output fragment_main<Policy : shading_policy>(fragment_input input) {{\n"
    "  lighting_output lighting_output = evaluate_lighting<Policy::receives_shadows, {0}_lighting_model>(build_lighting_input(input));\n"
    "\n"
    "  shading_input shading_input;\n"
    "  shading_input.lighting = lighting_output;\n"
    "  shading_input.depth = input.sv_position.z;\n"
    "\n"
    "  return Policy::shade(shading_input);\n"
    "}}\n",
    graph_name, direct_body, extra_body
  );

  return shader_graph_codegen_result{true, source, {}};
}

} // namespace sbx::assets
