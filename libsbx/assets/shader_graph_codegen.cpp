// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/shader_graph_codegen.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>

namespace sbx::assets {

// Which stage a graph is being walked for -- see codegen_context_of below. Every node but the
// three vertex-only/four fragment-only input nodes is valid in either.
enum class codegen_context : std::uint8_t {
  vertex,
  fragment
}; // enum class codegen_context

static auto codegen_context_of(shader_node_type type) -> std::optional<codegen_context> {
  switch (type) {
    case shader_node_type::input_vertex_position:
    case shader_node_type::input_vertex_normal:
    case shader_node_type::input_vertex_tangent:
      return codegen_context::vertex;
    case shader_node_type::input_uv:
    case shader_node_type::input_normal:
    case shader_node_type::input_view_dir:
    // Fresnel Effect's unconnected Normal/View Dir default to this fragment's own `n`/`view_dir`
    // locals (see _emit's fresnel_effect case) -- those only exist in fragment_main's scope, so the
    // whole node is fragment-only, same restriction as input_normal/input_view_dir themselves.
    case shader_node_type::fresnel_effect:
    // Scene Depth's unconnected UV defaults to this fragment's own screen position, and Screen
    // Position IS that same screen position -- there's no meaningful screen-space UV in the vertex
    // stage for either.
    case shader_node_type::scene_depth:
    case shader_node_type::screen_position:
    // Backed by a local declared only in fragment_main's scope (see _emit's own case and the
    // fragment_main/preview fragment_main templates' "world_position" declarations) -- the vertex
    // stage has no model matrix available to compute a world position from in the first place.
    case shader_node_type::input_world_position:
      return codegen_context::fragment;
    default:
      return std::nullopt; // valid in either context -- camera_position/main_light_direction/
                            // main_light_color/time/delta_time included: they read straight off the
                            // global `push` pointer, which every generated function has access to.
  }
}

// This fragment's own normalized [0,1] screen UV -- Scene Depth's own unconnected-UV default AND
// Screen Position's "default" mode are the exact same value, so both _emit cases share this one
// literal instead of two copies quietly drifting apart. Neither preview has a real frame_data to
// compute this from -- guarded by each case's own _scene_depth_preview_value/no-preview-support
// check before ever reaching this.
static constexpr auto screen_uv_expression = "(input.sv_position.xy / push.frame_data->render_target_size)";

static auto component_count(const std::string& slang_type) -> std::size_t {
  if (slang_type == "float2") return 2u;
  if (slang_type == "float3") return 3u;
  if (slang_type == "float4") return 4u;
  return 1u; // "float", or anything unrecognized
}

// Returns the reason the graph is over budget, or nullopt if it isn't.
static auto validate_shader_graph_parameter_counts(const shader_graph::create_info& graph) -> std::optional<std::string> {
  auto float_slots = std::uint32_t{0u};
  auto texture_slots = std::uint32_t{0u};

  for (const auto& parameter : compute_shader_graph_parameters(graph.nodes)) {
    if (parameter.type == shader_graph_parameter_type::texture_value) {
      ++texture_slots;
    } else {
      ++float_slots;
    }
  }

  // compute_shader_graph_parameters itself never caps these -- an over-budget graph would otherwise
  // silently emit an out-of-bounds material.generic_params[]/generic_textures[] index into the
  // generated Slang (undefined GPU behavior, not caught anywhere downstream), so this is the one
  // place that actually enforces the shader_graph_max_params/shader_graph_max_textures budget.
  if (float_slots > shader_graph_max_params) {
    return fmt::format("graph exposes {} float/vector3/color parameters, more than the {} allowed", float_slots, shader_graph_max_params);
  }

  if (texture_slots > shader_graph_max_textures) {
    return fmt::format("graph has {} Texture Sample nodes, more than the {} allowed", texture_slots, shader_graph_max_textures);
  }

  return std::nullopt;
}

// Validates every edge in the graph against shader_graph_type_resolver, plus Swizzle's own stored
// pattern and the exposed-parameter budget (validate_shader_graph_parameter_counts) -- the editor
// already rejects a mistyped connection/malformed pattern/over-budget graph, but a hand-edited or
// older-format .shadergraph file on disk isn't guaranteed to have gone through that, so codegen
// re-checks rather than trusting it (and emitting Slang that either doesn't compile or, worse,
// silently does the wrong thing). Returns the first problem found, or nullopt if the graph is clean.
static auto validate_shader_graph_types(const shader_graph::create_info& graph) -> std::optional<std::string> {
  if (auto error = validate_shader_graph_parameter_counts(graph)) {
    return error;
  }

  auto resolver = shader_graph_type_resolver{graph.nodes, graph.edges};

  for (const auto& edge : graph.edges) {
    const auto source_type = resolver.output_type(edge.from_node, edge.from_pin);

    if (!resolver.accepts(edge.to_node, edge.to_pin, source_type)) {
      return fmt::format("node {} input {} does not accept the type wired into it from node {}", edge.to_node, edge.to_pin, edge.from_node);
    }
  }

  for (const auto& node : graph.nodes) {
    if (node.type != shader_node_type::swizzle) {
      continue;
    }

    const auto pattern = std::holds_alternative<std::string>(node.value) ? std::get<std::string>(node.value) : std::string{};

    if (pattern.size() != 4u) {
      return fmt::format("node {} (Swizzle) pattern must be exactly 4 characters (r/g/b/a)", node.id);
    }

    for (const auto c : pattern) {
      if (c != 'r' && c != 'g' && c != 'b' && c != 'a') {
        return fmt::format("node {} (Swizzle) pattern may only contain r/g/b/a", node.id);
      }
    }
  }

  return std::nullopt;
}

struct emitted_value {
  std::string expression;
  std::string type; // Slang type, e.g. "float3", or "" for an intentionally-unconnected optional pin
}; // struct emitted_value

// A node can have several output pins (Split's R/G/B/A, Combine's RG/RGB/RGBA); every other node
// has exactly one. Cache and edge lookups are keyed by (node, pin) throughout so the two don't collide.
static auto pin_key(std::uint32_t node_id, std::uint32_t pin) -> std::uint64_t {
  return (static_cast<std::uint64_t>(node_id) << 32u) | pin;
}

// A binary/dominant-pin math node's result type: whichever of its operands isn't a scalar (Slang
// broadcasts the scalar one against it natively, no swizzle/cast needed at the text level); when
// both are non-scalar but differ (only float4-vs-float3 is possible -- accepts()'s "color degrades
// to rgb" exception), the narrower one wins, matching what dropping alpha means; scalar if both are.
static auto binary_type(const emitted_value& a, const emitted_value& b) -> const std::string& {
  if (a.type == "float") return b.type;
  if (b.type == "float") return a.type;
  if (a.type != b.type) return a.type == "float4" ? b.type : a.type;
  return a.type;
}

// Coerces an already-emitted value to `target_type` -- the only case this actually changes
// anything is a float4 source feeding a float3-required slot (accepts()'s "color degrades to rgb"
// exception; drops alpha via .rgb). Every other pairing validate_shader_graph_types already
// guarantees is either an exact match or a scalar (which Slang broadcasts natively, untouched).
static auto coerce_to(const emitted_value& value, const std::string& target_type) -> std::string {
  if (value.type == "float4" && target_type == "float3") {
    return fmt::format("{}.rgb", value.expression);
  }
  return value.expression;
}

// Concatenates a walker's accumulated `float3 add_12 = (...);`-style local-variable declarations
// (see shader_graph_codegen_walker::statements) into the preamble block a generated function body
// prepends before actually using any of them.
static auto join_statements(const std::vector<std::string>& statements) -> std::string {
  auto joined = std::string{};
  for (const auto& statement : statements) {
    joined += statement;
  }
  return joined;
}

// A generated file only #include's shader_graph_noise.slang (shader_graph_noise_random/_value/
// shader_graph_simple_noise/shader_graph_voronoi_random/shader_graph_voronoi -- the actual
// implementations, in exactly one place) when the graph actually has one of those two node types, so
// a graph that doesn't use noise gets no unused-function bloat/warnings and no unnecessary #include.
static auto noise_helper_source_for(const shader_graph::create_info& graph) -> std::string {
  const auto uses_noise = std::ranges::any_of(graph.nodes, [](const shader_graph_node& node) {
    return node.type == shader_node_type::simple_noise || node.type == shader_node_type::voronoi;
  });
  return uses_noise ? std::string{"#include <shader_graph_noise.slang>\n"} : std::string{};
}

// Walks a graph backward from one socket's input pin, producing a single self-contained Slang
// expression for it (a post-order DFS -- a node's dependencies are always resolved, and so
// inlined, before the node itself references them). Doubles as reachability (nodes never visited
// never appear in the output -- dead-code elimination as a side effect of the walk, not a separate
// pass) and cycle detection (a node re-entered while still on the visiting stack is a cycle).
//
// Every emitted node gets its own named local variable (e.g. `float3 add_12 = (...);`) rather than
// being inlined into one giant nested expression -- this is what makes the generated Slang readable
// enough to actually debug (matches Unity Shader Graph's own generated-code style), and it's also
// how a node referenced from two sockets this same walker visits (e.g. one Texture Sample feeding
// both Albedo and Alpha) ends up genuinely computed once at runtime, not just deduplicated in the
// generated text: both sockets' expressions just reference the same variable name. `_emitted`
// memoizes by (node, pin) for the lifetime of one walker instance to make that sharing happen;
// `statements()` returns the declarations in dependency order (post-order DFS guarantees every
// variable is declared before anything references it) for the caller to prepend as a preamble.
// generate_shader_graph_source shares one walker across all of a stage's sockets that land in the
// SAME Slang function (all of Fragment's sockets do) for exactly this reason; the 3 Vertex block
// sockets compile to 3 SEPARATE functions, so each gets its own walker instead -- sharing local
// variables across function boundaries isn't possible, and per-function statement lists keep each
// function self-contained and independently readable. A node visited from two DIFFERENT walkers
// (i.e. crossing the vertex/fragment stage boundary, or between two Vertex-block sockets) is still
// evaluated separately in each, which is unavoidable.
class shader_graph_codegen_walker {

public:

  // `sampler_index_expression` is the one piece of texture_sample's emitted text that differs
  // between the real path (`push.sampler_index`, the default) and the master preview (which packs
  // its sampler index into a push-constant vec4's otherwise-unused .w component instead of its own
  // field, to fit its push_data in the same 128-byte budget -- see
  // generate_shader_graph_preview_source). `time_expression`/`delta_time_expression` are the same
  // idea for the Time/Delta Time nodes -- the master preview's push_data is too tight on its
  // 128-byte budget to spend a whole field on Delta Time (a fixed literal instead -- see
  // generate_shader_graph_preview_source's own doc comment for why that's an acceptable
  // simplification for a preview specifically) and packs Time into another otherwise-unused .w.
  // `scene_depth_preview_value`/`screen_position_preview_value`, when non-empty, replace Scene
  // Depth's/Screen Position's ENTIRE real computation with that literal expression -- neither
  // preview kind has a real per-frame frame_data (the master preview's own push_data is a small
  // standalone struct, not a frame_data*; the node preview has no frame_data at all), so there is no
  // whole-scene depth or real render-target size to compute either node from in the first place, not
  // just differently-named fields. Fixed placeholder values are reasonable stand-ins, matching the
  // same "no real surface" tradeoff generate_node_preview_source already makes for Normal/View Dir.
  // Every other node type's emission is identical either way.
  shader_graph_codegen_walker(const shader_graph::create_info& graph, codegen_context context, std::string sampler_index_expression = "push.sampler_index", std::string time_expression = "push.time", std::string delta_time_expression = "push.delta_time", std::string scene_depth_preview_value = "", std::string screen_position_preview_value = "")
  : _context{context}, _sampler_index_expression{std::move(sampler_index_expression)}, _time_expression{std::move(time_expression)}, _delta_time_expression{std::move(delta_time_expression)}, _scene_depth_preview_value{std::move(scene_depth_preview_value)}, _screen_position_preview_value{std::move(screen_position_preview_value)} {
    for (const auto& node : graph.nodes) {
      _nodes_by_id.emplace(node.id, &node);
    }

    for (const auto& edge : graph.edges) {
      _incoming.emplace(pin_key(edge.to_node, edge.to_pin), pin_key(edge.from_node, edge.from_pin));
    }

    for (const auto& parameter : compute_shader_graph_parameters(graph.nodes)) {
      if (parameter.type == shader_graph_parameter_type::texture_value) {
        _texture_slot_of.emplace(parameter.node_id, parameter.slot);
      } else {
        _float_slot_of.emplace(parameter.node_id, parameter.slot);
      }
    }
  }

  // Returns nullopt if `pin` on `target_node` has nothing connected (not an error -- the caller
  // decides whether that socket has a default or is required) or if `error()` is set (check that
  // after every call).
  [[nodiscard]] auto visit_pin(std::uint32_t target_node, std::uint32_t pin) -> std::optional<emitted_value> {
    const auto entry = _incoming.find(pin_key(target_node, pin));

    if (entry == _incoming.end()) {
      return std::nullopt;
    }

    return _visit(static_cast<std::uint32_t>(entry->second >> 32u), static_cast<std::uint32_t>(entry->second));
  }

  [[nodiscard]] auto error() const -> const std::string& {
    return _error;
  }

  // Every emitted node's `  {type} {name} = {expression};\n` declaration, in dependency order --
  // the caller prepends these, joined, as a preamble before the function body that actually
  // references them (see this class's own doc comment).
  [[nodiscard]] auto statements() const -> const std::vector<std::string>& {
    return _statements;
  }

private:

  // A pin allowed to be left unconnected without that being an error -- texture_sample's uv (falls
  // back to the calling stage's own uv), every one of Combine's R/G/B/A (defaults to a literal 0,
  // matching Unity Shader Graph's own Combine node), Remap's In Min Max/Out Min Max (each defaults
  // to (0,1) -- pin 0, In, is still required), Fresnel Effect's Normal/View Dir/Power (default
  // to this fragment's own N/V and a power of 1), and every one of Simple Noise's/Voronoi's own pins
  // (UV falls back the same way texture_sample's does; Scale/Angle Offset/Cell Density each default
  // to a literal).
  [[nodiscard]] static auto _allows_unconnected(shader_node_type type, std::uint32_t pin) -> bool {
    if (type == shader_node_type::texture_sample && pin == 0u) return true;
    if (type == shader_node_type::combine) return true;
    if (type == shader_node_type::remap && pin != 0u) return true;
    if (type == shader_node_type::fresnel_effect) return true;
    if (type == shader_node_type::simple_noise) return true;
    if (type == shader_node_type::voronoi) return true;
    if (type == shader_node_type::scene_depth) return true; // UV defaults to this fragment's own screen position
    return false;
  }

  [[nodiscard]] auto _visit(std::uint32_t node_id, std::uint32_t pin) -> std::optional<emitted_value> {
    if (!_error.empty()) {
      return std::nullopt;
    }

    const auto key = pin_key(node_id, pin);

    if (const auto cached = _emitted.find(key); cached != _emitted.end()) {
      return cached->second;
    }

    if (_visiting.contains(key)) {
      _error = fmt::format("cycle detected at node {}", node_id);
      return std::nullopt;
    }

    const auto entry = _nodes_by_id.find(node_id);

    if (entry == _nodes_by_id.end()) {
      _error = fmt::format("edge references unknown node {}", node_id);
      return std::nullopt;
    }

    const auto& node = *entry->second;

    if (const auto required = codegen_context_of(node.type); required && *required != _context) {
      _error = fmt::format(
        "node {} ({}) is only valid in the {} stage",
        node_id, shader_node_type_to_string(node.type), *required == codegen_context::vertex ? "Vertex" : "Fragment"
      );
      return std::nullopt;
    }

    _visiting.insert(key);

    const auto input_count = shader_node_input_count(node.type);
    auto inputs = std::vector<emitted_value>{};
    inputs.reserve(input_count);

    for (auto input_pin = std::uint32_t{0u}; input_pin < input_count; ++input_pin) {
      const auto source = _incoming.find(pin_key(node_id, input_pin));

      if (source == _incoming.end()) {
        if (_allows_unconnected(node.type, input_pin)) {
          inputs.push_back(emitted_value{"", ""});
          continue;
        }

        _error = fmt::format("node {} ({}) input {} is not connected", node_id, shader_node_type_to_string(node.type), input_pin);
        return std::nullopt;
      }

      const auto value = _visit(static_cast<std::uint32_t>(source->second >> 32u), static_cast<std::uint32_t>(source->second));

      if (!value) {
        return std::nullopt;
      }

      inputs.push_back(*value);
    }

    _visiting.erase(key);

    const auto result = _emit(node, pin, inputs);

    if (!_error.empty()) {
      return std::nullopt;
    }

    // Named after the node's type + id (plus the output pin, for a multi-output node like Split/
    // Combine where pin 0 alone wouldn't disambiguate) rather than a bare counter, so the generated
    // Slang reads as e.g. `float3 add_12 = (...)` instead of `float3 tmp7 = (...)` -- the whole
    // point of this is to make the output debuggable.
    const auto var_name = pin == 0u
      ? fmt::format("{}_{}", shader_node_type_to_string(node.type), node_id)
      : fmt::format("{}_{}_{}", shader_node_type_to_string(node.type), node_id, pin);

    _statements.push_back(fmt::format("  {} {} = {};\n", result.type, var_name, result.expression));

    const auto materialized = emitted_value{var_name, result.type};

    _emitted.emplace(key, materialized);

    return materialized;
  }

  [[nodiscard]] auto _emit(const shader_graph_node& node, std::uint32_t output_pin, const std::vector<emitted_value>& inputs) -> emitted_value {
    switch (node.type) {
      case shader_node_type::input_uv: return {"uv", "float2"};
      case shader_node_type::input_normal: return {"n", "float3"};
      case shader_node_type::input_view_dir: return {"view_dir", "float3"};
      case shader_node_type::input_world_position: return {"world_position", "float3"};
      case shader_node_type::input_vertex_position: return {"v.position", "float3"};
      case shader_node_type::input_vertex_normal: return {"v.normal", "float3"};
      case shader_node_type::input_vertex_tangent: return {"v.tangent.xyz", "float3"};

      case shader_node_type::camera_position: return {"push.frame_data->camera_position.xyz", "float3"};

      // Directional light 0 is "the main light" -- matches evaluate_lit_surface's own convention
      // (lighting.slang: `for (i < directional_light_count) ... if (i == 0u) { shadow... }`), so a
      // graph reading this gets the exact same light a Fragment (Lit) graph is already implicitly
      // shaded by. This is the light's own travel direction (light -> surface), matching Unity
      // Shader Graph's Main Light Direction node exactly -- NOT the surface-to-light vector
      // lighting.slang's own `l` uses internally (that's this, negated); a graph wanting N.L wires
      // this through a Negate node first, same as a Unity toon-shader graph ported over would
      // already do. (0,-1,0) (a sun shining straight down) when the scene has no directional light,
      // rather than an out-of-bounds lights[0] read.
      case shader_node_type::main_light_direction:
        return {"(push.frame_data->directional_light_count > 0u ? normalize(push.frame_data->lights[0].direction.xyz) : float3(0.0, -1.0, 0.0))", "float3"};
      case shader_node_type::main_light_color:
        return {"(push.frame_data->directional_light_count > 0u ? push.frame_data->lights[0].color.rgb * push.frame_data->lights[0].color.a : float3(0.0, 0.0, 0.0))", "float3"};

      case shader_node_type::time: return {_time_expression, "float"};
      case shader_node_type::delta_time: return {_delta_time_expression, "float"};

      case shader_node_type::scene_depth: {
        // Neither preview has a real frame_data to sample from -- see this class's own doc comment
        // on scene_depth_preview_value.
        if (!_scene_depth_preview_value.empty()) {
          return {_scene_depth_preview_value, "float"};
        }

        const auto& uv = inputs[0].expression.empty() ? std::string{screen_uv_expression} : inputs[0].expression;
        const auto raw = fmt::format("textures[push.frame_data->scene_depth_index].Sample(samplers[{}], {}).r", _sampler_index_expression, uv);

        const auto mode = shader_node_scene_depth_mode(node);

        if (mode == "raw") {
          return {raw, "float"};
        }

        // Standard (non-reversed-Z) [0,1]-depth-range linearization -- matches this engine's own
        // perspective projection convention exactly (see math::matrix4x4::perspective's own doc
        // comment).
        const auto eye = fmt::format("((push.frame_data->near_plane * push.frame_data->far_plane) / (push.frame_data->far_plane - ({}) * (push.frame_data->far_plane - push.frame_data->near_plane)))", raw);

        if (mode == "eye") {
          return {eye, "float"};
        }

        return {fmt::format("(({}) / push.frame_data->far_plane)", eye), "float"}; // linear01
      }

      case shader_node_type::screen_position: {
        // Same "no real frame_data in a preview" reasoning as scene_depth above.
        if (!_screen_position_preview_value.empty()) {
          return {_screen_position_preview_value, "float4"};
        }

        const auto mode = shader_node_screen_position_mode(node);

        if (mode == "raw") {
          return {"input.sv_position", "float4"}; // window-space xy, hardware depth z, 1/clip-w in w
        }

        return {fmt::format("float4({}, input.sv_position.z, input.sv_position.w)", screen_uv_expression), "float4"};
      }

      case shader_node_type::constant_float: {
        if (node.exposed) {
          return {fmt::format("material.generic_params[{}].x", _float_slot_of.at(node.id)), "float"};
        }
        const auto value = std::holds_alternative<std::float_t>(node.value) ? std::get<std::float_t>(node.value) : 0.0f;
        return {fmt::format("float({})", value), "float"};
      }

      case shader_node_type::constant_vector2: {
        if (node.exposed) {
          return {fmt::format("material.generic_params[{}].xy", _float_slot_of.at(node.id)), "float2"};
        }
        const auto value = std::holds_alternative<math::vector2>(node.value) ? std::get<math::vector2>(node.value) : math::vector2{};
        return {fmt::format("float2({}, {})", value.x(), value.y()), "float2"};
      }

      case shader_node_type::constant_vector3: {
        if (node.exposed) {
          return {fmt::format("material.generic_params[{}].xyz", _float_slot_of.at(node.id)), "float3"};
        }
        const auto value = std::holds_alternative<math::vector3>(node.value) ? std::get<math::vector3>(node.value) : math::vector3{};
        return {fmt::format("float3({}, {}, {})", value.x(), value.y(), value.z()), "float3"};
      }

      case shader_node_type::constant_vector4: {
        if (node.exposed) {
          return {fmt::format("material.generic_params[{}]", _float_slot_of.at(node.id)), "float4"};
        }
        const auto value = std::holds_alternative<math::vector4>(node.value) ? std::get<math::vector4>(node.value) : math::vector4{};
        return {fmt::format("float4({}, {}, {}, {})", value.x(), value.y(), value.z(), value.w()), "float4"};
      }

      case shader_node_type::constant_color: {
        if (node.exposed) {
          return {fmt::format("material.generic_params[{}]", _float_slot_of.at(node.id)), "float4"};
        }
        const auto value = std::holds_alternative<math::color>(node.value) ? std::get<math::color>(node.value) : math::color{};
        return {fmt::format("float4({}, {}, {}, {})", value.r(), value.g(), value.b(), value.a()), "float4"};
      }

      case shader_node_type::texture_sample: {
        // ponytail: node.value (the node's own "default" texture, settable in the graph editor's
        // node inspector) is never read here -- an unset material slot always samples
        // asset_residency's plain white fallback, not this node's default. Revisit if a graph
        // author actually needs a per-graph default.
        //
        // 5 output pins (RGBA, R, G, B, A -- see shader_node_output_count) all read the SAME
        // underlying texture fetch, so the actual `.Sample(...)` call is cached per NODE (not per
        // pin, unlike every other node type) in _texture_sample_variable and only emitted once, the
        // first time any of this node's pins is visited -- every other pin then just swizzles that
        // one already-materialized float4 variable. Without this, wiring e.g. both RGB (Albedo) and
        // A (Alpha) from one Sample Texture node -- exactly the case this feature exists for --
        // would silently re-sample the same texture twice.
        const auto slot = _texture_slot_of.at(node.id);
        const auto default_uv = _context == codegen_context::fragment ? std::string{"uv"} : std::string{"v.uv"};
        const auto& uv = inputs[0].expression.empty() ? default_uv : inputs[0].expression;

        const auto cached = _texture_sample_variable.find(node.id);
        auto rgba_variable = std::string{};

        if (cached != _texture_sample_variable.end()) {
          rgba_variable = cached->second;
        } else {
          rgba_variable = fmt::format("texture_sample_{}_rgba", node.id);
          _statements.push_back(fmt::format("  float4 {} = textures[material.generic_textures[{}]].Sample(samplers[{}], {});\n", rgba_variable, slot, _sampler_index_expression, uv));
          _texture_sample_variable.emplace(node.id, rgba_variable);
        }

        if (output_pin == 0u) {
          return {rgba_variable, "float4"};
        }

        static constexpr auto swizzles = std::array<const char*, 4u>{"r", "g", "b", "a"};
        return {fmt::format("{}.{}", rgba_variable, swizzles[output_pin - 1u]), "float"};
      }

      case shader_node_type::add:
      case shader_node_type::subtract:
      case shader_node_type::multiply:
      case shader_node_type::divide: {
        const auto& t = binary_type(inputs[0], inputs[1]);
        const auto* op = node.type == shader_node_type::add ? "+" : node.type == shader_node_type::subtract ? "-" : node.type == shader_node_type::multiply ? "*" : "/";
        return {fmt::format("({} {} {})", coerce_to(inputs[0], t), op, coerce_to(inputs[1], t)), t};
      }

      case shader_node_type::lerp: {
        const auto& t = binary_type(inputs[0], inputs[1]);
        return {fmt::format("lerp({}, {}, {})", coerce_to(inputs[0], t), coerce_to(inputs[1], t), coerce_to(inputs[2], t)), t};
      }

      case shader_node_type::dot: {
        const auto& t = binary_type(inputs[0], inputs[1]);
        return {fmt::format("dot({}, {})", coerce_to(inputs[0], t), coerce_to(inputs[1], t)), "float"};
      }

      case shader_node_type::cross:
        return {fmt::format("cross({}, {})", coerce_to(inputs[0], "float3"), coerce_to(inputs[1], "float3")), "float3"};

      case shader_node_type::normalize: return {fmt::format("normalize({})", inputs[0].expression), inputs[0].type};
      case shader_node_type::saturate: return {fmt::format("saturate({})", inputs[0].expression), inputs[0].type};

      case shader_node_type::pow: {
        const auto& t = inputs[0].type; // base alone drives the type (matches the resolver's dominant_pins={0} rule)
        return {fmt::format("pow({}, {})", inputs[0].expression, coerce_to(inputs[1], t)), t};
      }

      case shader_node_type::step: {
        const auto& t = binary_type(inputs[0], inputs[1]);
        return {fmt::format("step({}, {})", coerce_to(inputs[0], t), coerce_to(inputs[1], t)), t};
      }

      case shader_node_type::smoothstep: {
        const auto& t = binary_type(inputs[0], inputs[1]);
        return {fmt::format("smoothstep({}, {}, {})", coerce_to(inputs[0], t), coerce_to(inputs[1], t), coerce_to(inputs[2], t)), t};
      }

      case shader_node_type::negate: return {fmt::format("(-{})", inputs[0].expression), inputs[0].type};
      case shader_node_type::one_minus: return {fmt::format("(1.0 - {})", inputs[0].expression), inputs[0].type};
      case shader_node_type::absolute: return {fmt::format("abs({})", inputs[0].expression), inputs[0].type};
      case shader_node_type::floor: return {fmt::format("floor({})", inputs[0].expression), inputs[0].type};
      case shader_node_type::ceiling: return {fmt::format("ceil({})", inputs[0].expression), inputs[0].type};
      case shader_node_type::round: return {fmt::format("round({})", inputs[0].expression), inputs[0].type};
      case shader_node_type::fraction: return {fmt::format("frac({})", inputs[0].expression), inputs[0].type};
      case shader_node_type::sign: return {fmt::format("sign({})", inputs[0].expression), inputs[0].type};
      case shader_node_type::sine: return {fmt::format("sin({})", inputs[0].expression), inputs[0].type};
      case shader_node_type::cosine: return {fmt::format("cos({})", inputs[0].expression), inputs[0].type};

      case shader_node_type::minimum: {
        const auto& t = binary_type(inputs[0], inputs[1]);
        return {fmt::format("min({}, {})", coerce_to(inputs[0], t), coerce_to(inputs[1], t)), t};
      }

      case shader_node_type::maximum: {
        const auto& t = binary_type(inputs[0], inputs[1]);
        return {fmt::format("max({}, {})", coerce_to(inputs[0], t), coerce_to(inputs[1], t)), t};
      }

      case shader_node_type::clamp: {
        const auto& t = inputs[0].type; // In alone drives the type (dominant_pins={0}, same as Pow)
        return {fmt::format("clamp({}, {}, {})", inputs[0].expression, coerce_to(inputs[1], t), coerce_to(inputs[2], t)), t};
      }

      case shader_node_type::length:
        return {fmt::format("length({})", inputs[0].expression), "float"};

      case shader_node_type::distance: {
        const auto& t = binary_type(inputs[0], inputs[1]);
        return {fmt::format("distance({}, {})", coerce_to(inputs[0], t), coerce_to(inputs[1], t)), "float"};
      }

      case shader_node_type::reflect:
        return {fmt::format("reflect({}, {})", inputs[0].expression, inputs[1].expression), "float3"};

      case shader_node_type::remap: {
        // In Min Max/Out Min Max each independently default to (0,1) when unconnected
        // (_allows_unconnected) -- a float2 range rather than two separate scalars since they're
        // always a matched pair, same reasoning Unity Shader Graph's own Remap node already has
        // its In Min Max/Out Min Max as single Vector2 ports rather than 4 separate float fields.
        // No divide-by-zero guard on (in_range.y - in_range.x) -- matching Unity's own Remap node,
        // which doesn't guard it either (an equal min/max is the graph author's own mistake to fix,
        // same as it would be there).
        const auto& t = inputs[0].type; // In alone drives the type (dominant_pins={0})
        const auto in_range = inputs[1].expression.empty() ? std::string{"float2(0.0, 1.0)"} : inputs[1].expression;
        const auto out_range = inputs[2].expression.empty() ? std::string{"float2(0.0, 1.0)"} : inputs[2].expression;
        return {fmt::format("({0}.x + ({1} - {2}.x) * ({0}.y - {0}.x) / ({2}.y - {2}.x))", out_range, inputs[0].expression, in_range), t};
      }

      case shader_node_type::fresnel_effect: {
        // Unconnected Normal/View Dir default to this fragment's own N/V (codegen_context_of
        // restricts this node to the fragment stage specifically so these locals are always in
        // scope); unconnected Power defaults to 1 -- matching Unity Shader Graph's own Fresnel node.
        const auto normal = inputs[0].expression.empty() ? std::string{"n"} : inputs[0].expression;
        const auto view_dir = inputs[1].expression.empty() ? std::string{"view_dir"} : inputs[1].expression;
        const auto power = inputs[2].expression.empty() ? std::string{"1.0"} : inputs[2].expression;
        return {fmt::format("pow(saturate(1.0 - dot(normalize({}), normalize({}))), {})", normal, view_dir, power), "float"};
      }

      case shader_node_type::simple_noise: {
        const auto default_uv = _context == codegen_context::fragment ? std::string{"uv"} : std::string{"v.uv"};
        const auto& uv = inputs[0].expression.empty() ? default_uv : inputs[0].expression;
        const auto scale = inputs[1].expression.empty() ? std::string{"500.0"} : inputs[1].expression;
        return {fmt::format("shader_graph_simple_noise({}, {})", uv, scale), "float"};
      }

      case shader_node_type::voronoi: {
        // Out and Cells share the same underlying search, so a graph wiring up both recomputes it
        // twice (once per output pin's own local variable) -- see shader_graph_codegen_walker's own
        // doc comment on per-(node,pin) variables. Fine in practice: wiring up both is rare, and the
        // search itself is only 9 taps.
        const auto default_uv = _context == codegen_context::fragment ? std::string{"uv"} : std::string{"v.uv"};
        const auto& uv = inputs[0].expression.empty() ? default_uv : inputs[0].expression;
        const auto angle_offset = inputs[1].expression.empty() ? std::string{"2.0"} : inputs[1].expression;
        const auto cell_density = inputs[2].expression.empty() ? std::string{"5.0"} : inputs[2].expression;
        const auto call = fmt::format("shader_graph_voronoi({}, {}, {})", uv, angle_offset, cell_density);
        return {fmt::format("{}.{}", call, output_pin == 0u ? "x" : "y"), "float"};
      }

      case shader_node_type::swizzle: {
        const auto pattern = shader_node_swizzle_pattern(node);
        const auto width = component_count(inputs[0].type);
        return {fmt::format("{}.{}", inputs[0].expression, pattern.substr(0u, width)), std::array<const char*, 4u>{"float", "float2", "float3", "float4"}[width - 1u]};
      }

      case shader_node_type::split: {
        // Always 4 outputs regardless of the input's actual width -- a component beyond it just
        // reads as a literal 0, matching Unity Shader Graph's own Split node exactly (no error).
        static constexpr auto swizzles = std::array<const char*, 4u>{"r", "g", "b", "a"};
        const auto width = component_count(inputs[0].type);
        if (output_pin >= width) {
          return {"0.0", "float"};
        }
        return {fmt::format("{}.{}", inputs[0].expression, swizzles[output_pin]), "float"};
      }

      case shader_node_type::combine: {
        // Every one of R/G/B/A is independently optional (_allows_unconnected), defaulting to a
        // literal 0 -- all 3 outputs (RG/RGB/RGBA) are always available regardless of how many are
        // actually wired, matching Unity Shader Graph's own Combine node exactly.
        const auto component = [&](std::size_t i) { return inputs[i].expression.empty() ? std::string{"0.0"} : inputs[i].expression; };
        if (output_pin == 0u) return {fmt::format("float2({}, {})", component(0u), component(1u)), "float2"};
        if (output_pin == 1u) return {fmt::format("float3({}, {}, {})", component(0u), component(1u), component(2u)), "float3"};
        return {fmt::format("float4({}, {}, {}, {})", component(0u), component(1u), component(2u), component(3u)), "float4"};
      }

      case shader_node_type::output_vertex:
      case shader_node_type::output_fragment_lit:
      case shader_node_type::output_fragment_unlit:
        return inputs[0]; // sinks are never reached here -- visit_pin() reads their inputs directly
    }

    return {"float3(0.0, 0.0, 0.0)", "float3"};
  }

  codegen_context _context;
  std::string _sampler_index_expression;
  std::string _time_expression;
  std::string _delta_time_expression;
  std::string _scene_depth_preview_value;
  std::string _screen_position_preview_value;
  std::unordered_map<std::uint32_t, const shader_graph_node*> _nodes_by_id{};
  std::unordered_map<std::uint64_t, std::uint64_t> _incoming{}; // pin_key(to) -> pin_key(from)
  std::unordered_map<std::uint32_t, std::uint32_t> _float_slot_of{};
  std::unordered_map<std::uint32_t, std::uint32_t> _texture_slot_of{};
  std::unordered_map<std::uint64_t, emitted_value> _emitted{};
  std::unordered_set<std::uint64_t> _visiting{};
  std::vector<std::string> _statements{};
  std::unordered_map<std::uint32_t, std::string> _texture_sample_variable{}; // node id -> its one shared RGBA sample variable, see _emit's texture_sample case
  std::string _error{};

}; // class shader_graph_codegen_walker

// One Vertex block socket (Position/Normal/Tangent) -- always emitted as its own free function,
// `vertex_main`/`depth_vertex_main` both call all three so a vertex-displacing graph gets
// consistent depth/shadow. No `output_vertex` node, or an unconnected pin, is not an error -- it
// just passes the corresponding attribute through unchanged, matching the engine's previous fixed
// vertex stage exactly.
//
// `walker` gets its own fresh instance per call (see generate_shader_graph_source) rather than being
// shared across all three vertex-component sockets -- each compiles to a SEPARATE Slang function, so
// a shared walker's local-variable declarations (see shader_graph_codegen_walker's own doc comment)
// couldn't be referenced across all three anyway. A node feeding more than one of Position/Normal/
// Tangent is walked, emitted, and declared once per function instead of truly once overall -- a
// deliberate simplification over merging all three into one function (which would need
// depth_vertex_main/shadow to needlessly compute Normal/Tangent alongside Position too).
//
// `material_declaration` is the one line that differs between the real path and the preview path
// (see generate_shader_graph_preview_source) -- both declare a local named `material` (so `_emit`'s
// own "material.generic_params[...]"/"material.generic_textures[...]" text needs no mode-awareness
// at all), just from a different expression (the real per-draw material buffer vs. the preview's
// own single-entry one). Only emitted when there's an actual expression to return -- an
// unconnected/no-Vertex-block function has nothing that could reference `material` anyway.
static auto emit_vertex_component(const shader_graph_node* vertex_node, std::uint32_t pin, shader_graph_codegen_walker& walker, const std::string& fn_name, const std::string& default_expression, const std::string& material_declaration) -> std::expected<std::string, std::string> {
  if (vertex_node == nullptr) {
    return fmt::format("float3 {}(vertex v) {{\n  return {};\n}}\n", fn_name, default_expression);
  }

  const auto value = walker.visit_pin(vertex_node->id, pin);

  if (!walker.error().empty()) {
    return std::unexpected{walker.error()};
  }

  if (!value) {
    return fmt::format("float3 {}(vertex v) {{\n  return {};\n}}\n", fn_name, default_expression);
  }

  // output_vertex's Position/Normal/Tangent pins are fixed float3 -- the only coercion
  // validate_shader_graph_types could have let through is a float4 (Color/Sample Texture) source,
  // dropping its alpha.
  return fmt::format(
    "float3 {}(vertex v) {{\n"
    "{}"
    "{}"
    "  return {};\n"
    "}}\n",
    fn_name, material_declaration, join_statements(walker.statements()), coerce_to(*value, "float3")
  );
}

// One Fragment block socket (Albedo, Metallic, Color, ...), evaluated in codegen_context::fragment
// where `material`/`uv`/`n`/`view_dir` are already in scope (fragment_main's own prologue -- see
// generate_shader_graph_source). Unconnected falls back to `default_expression`; pass an empty
// default to make the socket required instead (the returned std::expected holds the "not connected"
// error if it's left unconnected). A connected value is coerced with coerce_to against
// `required_type` -- every Fragment socket is a fixed type, so this only ever actually changes
// anything for the float4-Color-into-float3 case.
//
// `walker` is shared across every fragment-socket call for this graph (see
// generate_shader_graph_source) -- all of Lit's 8 sockets (or Unlit's 3) land in the same
// fragment_main function body, so a node feeding two of them (e.g. one Texture Sample driving both
// Albedo and Alpha) is walked and emitted exactly once instead of once per consuming socket.
static auto emit_fragment_socket(std::uint32_t sink_node_id, std::uint32_t pin, shader_graph_codegen_walker& walker, const std::string& required_type, const std::string& default_expression) -> std::expected<std::string, std::string> {
  const auto value = walker.visit_pin(sink_node_id, pin);

  if (!walker.error().empty()) {
    return std::unexpected{walker.error()};
  }

  if (!value) {
    if (default_expression.empty()) {
      return std::unexpected{fmt::format("node {} input {} is not connected", sink_node_id, pin)};
    }
    return default_expression;
  }

  return coerce_to(*value, required_type);
}

// Prefixes `emitted`'s error (if any) with `label` -- every emit_vertex_component/
// emit_fragment_socket call site needs its own label so a validation error names the socket it
// actually came from ("Fragment (Lit) Albedo: ...") rather than just the underlying node/pin.
static auto labeled(std::expected<std::string, std::string> emitted, std::string_view label) -> std::expected<std::string, std::string> {
  if (!emitted) {
    return std::unexpected{fmt::format("{}: {}", label, emitted.error())};
  }
  return emitted;
}

auto generate_shader_graph_source(const std::string& graph_name, const shader_graph::create_info& graph) -> std::expected<std::string, std::string> {
  const auto lit_it = std::ranges::find_if(graph.nodes, [](const shader_graph_node& node) { return node.type == shader_node_type::output_fragment_lit; });
  const auto unlit_it = std::ranges::find_if(graph.nodes, [](const shader_graph_node& node) { return node.type == shader_node_type::output_fragment_unlit; });
  const auto has_lit = lit_it != graph.nodes.end();
  const auto has_unlit = unlit_it != graph.nodes.end();

  if (!has_lit && !has_unlit) {
    return std::unexpected{"graph has no Fragment output -- add a Fragment (Lit) or Fragment (Unlit) node"};
  }

  if (has_lit && has_unlit) {
    return std::unexpected{"graph has both a Fragment (Lit) and a Fragment (Unlit) output -- exactly one is required"};
  }

  if (const auto error = validate_shader_graph_types(graph)) {
    return std::unexpected{*error};
  }

  const auto vertex_it = std::ranges::find_if(graph.nodes, [](const shader_graph_node& node) { return node.type == shader_node_type::output_vertex; });
  const auto* vertex_node = vertex_it != graph.nodes.end() ? &*vertex_it : nullptr;

  static constexpr auto real_material_declaration = "  material_data material = push.frame_data->materials[push.material_index];\n";

  auto vertex_position_walker = shader_graph_codegen_walker{graph, codegen_context::vertex};
  const auto vertex_position_fn = labeled(emit_vertex_component(vertex_node, 0u, vertex_position_walker, "graph_vertex_position", "v.position", real_material_declaration), "Vertex Position");
  if (!vertex_position_fn) return std::unexpected{vertex_position_fn.error()};

  auto vertex_normal_walker = shader_graph_codegen_walker{graph, codegen_context::vertex};
  const auto vertex_normal_fn = labeled(emit_vertex_component(vertex_node, 1u, vertex_normal_walker, "graph_vertex_normal", "v.normal", real_material_declaration), "Vertex Normal");
  if (!vertex_normal_fn) return std::unexpected{vertex_normal_fn.error()};

  auto vertex_tangent_walker = shader_graph_codegen_walker{graph, codegen_context::vertex};
  const auto vertex_tangent_fn = labeled(emit_vertex_component(vertex_node, 2u, vertex_tangent_walker, "graph_vertex_tangent", "v.tangent.xyz", real_material_declaration), "Vertex Tangent");
  if (!vertex_tangent_fn) return std::unexpected{vertex_tangent_fn.error()};

  auto fragment_walker = shader_graph_codegen_walker{graph, codegen_context::fragment};
  auto fragment_body = std::string{};

  // depth_fragment_main (below) needs the graph's own Alpha/Alpha Clip Threshold too, to actually
  // clip the same silhouette fragment_main does -- re-walked through a SEPARATE fresh walker (same
  // reasoning as the per-vertex-component walkers above: it's a different Slang function, so a
  // local variable declared for fragment_walker's own statements() isn't in scope there). Only
  // Alpha/Alpha Clip Threshold are needed; the rest of the socket set (Albedo, Normal, ...) never
  // affects whether a fragment survives, so re-evaluating just these two is enough.
  auto depth_fragment_walker = shader_graph_codegen_walker{graph, codegen_context::fragment};
  auto depth_alpha = std::string{};
  auto depth_alpha_clip_threshold = std::string{};

  if (has_lit) {
    const auto id = lit_it->id;

    const auto albedo = labeled(emit_fragment_socket(id, 0u, fragment_walker, "float3", "float3(1.0, 1.0, 1.0)"), "Fragment (Lit) Albedo");
    if (!albedo) return std::unexpected{albedo.error()};

    const auto normal = labeled(emit_fragment_socket(id, 1u, fragment_walker, "float3", "float3(0.0, 0.0, 1.0)"), "Fragment (Lit) Normal");
    if (!normal) return std::unexpected{normal.error()};

    const auto metallic = labeled(emit_fragment_socket(id, 2u, fragment_walker, "float", "0.0"), "Fragment (Lit) Metallic");
    if (!metallic) return std::unexpected{metallic.error()};

    const auto roughness = labeled(emit_fragment_socket(id, 3u, fragment_walker, "float", "0.5"), "Fragment (Lit) Roughness");
    if (!roughness) return std::unexpected{roughness.error()};

    const auto emission = labeled(emit_fragment_socket(id, 4u, fragment_walker, "float3", "float3(0.0, 0.0, 0.0)"), "Fragment (Lit) Emission");
    if (!emission) return std::unexpected{emission.error()};

    const auto occlusion = labeled(emit_fragment_socket(id, 5u, fragment_walker, "float", "1.0"), "Fragment (Lit) Occlusion");
    if (!occlusion) return std::unexpected{occlusion.error()};

    const auto alpha = labeled(emit_fragment_socket(id, 6u, fragment_walker, "float", "1.0"), "Fragment (Lit) Alpha");
    if (!alpha) return std::unexpected{alpha.error()};

    const auto alpha_clip_threshold = labeled(emit_fragment_socket(id, 7u, fragment_walker, "float", "material.alpha_cutoff"), "Fragment (Lit) Alpha Clip Threshold");
    if (!alpha_clip_threshold) return std::unexpected{alpha_clip_threshold.error()};

    fragment_body = join_statements(fragment_walker.statements()) + fmt::format(
      "  lit_surface surf;\n"
      "  surf.albedo = {0};\n"
      "  surf.tangent_space_normal = {1};\n"
      "  surf.metallic = {2};\n"
      "  surf.roughness = {3};\n"
      "  surf.emissive = {4};\n"
      "  surf.ao = {5};\n"
      "  surf.alpha = {6};\n"
      "  surf.dielectric_f0 = 0.04;\n"
      "  surf.receives_shadow = true;\n"
      "\n"
      "  lighting_output lighting_output = evaluate_lit_surface<Policy::receives_shadows>(lighting_input, surf);\n"
      "\n"
      "  uint is_alpha_masked = (material.flags & material_flags::alpha_masked) != 0u;\n"
      "  clip(is_alpha_masked * ({6} - {7}));\n",
      *albedo, *normal, *metallic, *roughness, *emission, *occlusion, *alpha, *alpha_clip_threshold
    );

    const auto depth_alpha_result = labeled(emit_fragment_socket(id, 6u, depth_fragment_walker, "float", "1.0"), "Fragment (Lit) Alpha");
    if (!depth_alpha_result) return std::unexpected{depth_alpha_result.error()};
    depth_alpha = *depth_alpha_result;

    const auto depth_alpha_clip_result = labeled(emit_fragment_socket(id, 7u, depth_fragment_walker, "float", "material.alpha_cutoff"), "Fragment (Lit) Alpha Clip Threshold");
    if (!depth_alpha_clip_result) return std::unexpected{depth_alpha_clip_result.error()};
    depth_alpha_clip_threshold = *depth_alpha_clip_result;
  } else {
    const auto id = unlit_it->id;

    const auto color = labeled(emit_fragment_socket(id, 0u, fragment_walker, "float3", ""), "Fragment (Unlit) Color");
    if (!color) return std::unexpected{color.error()};

    const auto alpha = labeled(emit_fragment_socket(id, 1u, fragment_walker, "float", "1.0"), "Fragment (Unlit) Alpha");
    if (!alpha) return std::unexpected{alpha.error()};

    const auto alpha_clip_threshold = labeled(emit_fragment_socket(id, 2u, fragment_walker, "float", "material.alpha_cutoff"), "Fragment (Unlit) Alpha Clip Threshold");
    if (!alpha_clip_threshold) return std::unexpected{alpha_clip_threshold.error()};

    fragment_body = join_statements(fragment_walker.statements()) + fmt::format(
      "  lighting_output lighting_output;\n"
      "  lighting_output.color = {0};\n"
      "  lighting_output.alpha = {1};\n"
      "\n"
      "  uint is_alpha_masked = (material.flags & material_flags::alpha_masked) != 0u;\n"
      "  clip(is_alpha_masked * ({1} - {2}));\n",
      *color, *alpha, *alpha_clip_threshold
    );

    const auto depth_alpha_result = labeled(emit_fragment_socket(id, 1u, depth_fragment_walker, "float", "1.0"), "Fragment (Unlit) Alpha");
    if (!depth_alpha_result) return std::unexpected{depth_alpha_result.error()};
    depth_alpha = *depth_alpha_result;

    const auto depth_alpha_clip_result = labeled(emit_fragment_socket(id, 2u, depth_fragment_walker, "float", "material.alpha_cutoff"), "Fragment (Unlit) Alpha Clip Threshold");
    if (!depth_alpha_clip_result) return std::unexpected{depth_alpha_clip_result.error()};
    depth_alpha_clip_threshold = *depth_alpha_clip_result;
  }

  const auto depth_fragment_body = join_statements(depth_fragment_walker.statements());

  auto source = fmt::format(
    "// AUTO-GENERATED by shader graph codegen (shader_graph: {0}). Do not hand-edit -- edit the .shadergraph source and re-cook.\n"
    // Root-relative, not bare -- the generated file lives in shaders/generated/, whose own search
    // path doesn't cover shaders/pbr/ where geometry_common.slang actually is; the shaders/ root
    // itself is always a search path (shader_compiler.cpp's _shaders_root), so this resolves
    // regardless of which subdirectory the generated file ends up in.
    "#include <pbr/geometry_common.slang>\n"
    "\n"
    "{5}"
    "{1}"
    "\n"
    "{2}"
    "\n"
    "{3}"
    "\n"
    "[shader(\"vertex\")]\n"
    "vertex_output vertex_main(vertex_input input) {{\n"
    "  vertex v = push.vertices[input.vertex_id];\n"
    "  frame_data frame_data = *push.frame_data;\n"
    "\n"
    "  transform_data instance = push.transforms[push.transform_offset + input.instance_id];\n"
    "\n"
    "  float3 object_position = graph_vertex_position(v);\n"
    "  float3 object_normal = graph_vertex_normal(v);\n"
    "  float3 object_tangent = graph_vertex_tangent(v);\n"
    "\n"
    "  float4 world_position = mul(instance.model, float4(object_position, 1.0));\n"
    "\n"
    "  vertex_output output;\n"
    "  output.position = world_position.xyz;\n"
    "  output.normal = normalize(mul(instance.normal, float4(object_normal, 0.0)).xyz);\n"
    "  output.tangent = float4(normalize(mul(instance.normal, float4(object_tangent, 0.0)).xyz), v.tangent.w);\n"
    "  output.uv = v.uv;\n"
    "  output.sv_position = mul(frame_data.projection, mul(frame_data.view, world_position));\n"
    "\n"
    "  return output;\n"
    "}}\n"
    "\n"
    // Depth pre-pass/shadow pass entry points -- always emitted (not just when the graph has a
    // Vertex block) so both passes can unconditionally use this graph's own pipeline instead of
    // special-casing "does this graph touch vertices"; a graph with no Vertex block just gets the
    // same pass-through position here as vertex_main does above, matching the built-in depth
    // shaders exactly. See shaders/passes/depth_pre.slang / shadow.slang for the shape this mirrors.
    // depth_fragment_main re-derives the graph's own Alpha/Alpha Clip Threshold (depth_alpha/
    // depth_alpha_clip_threshold, computed above alongside fragment_main's own) rather than the
    // real material's fixed apply_alpha_cutout -- opaque_pass runs with depth_write=false and a
    // less_or_equal compare against this depth pre-pass (see opaque_pass.cpp), so a mismatch here
    // used to leave fragment_main's own clip() correctly discarding a masked pixel's COLOR while
    // this pass had already written its DEPTH, occluding whatever should have shown through the
    // hole (background/other geometry) with nothing -- the "mask renders as an opaque black hole"
    // bug. `n`/`view_dir` are fixed placeholders (no real surface normal is available this cheaply
    // in a depth-only pass) -- fine for the overwhelmingly common case (Alpha driven by a Texture
    // Sample/constant), same tradeoff generate_node_preview_source already makes for the same reason.
    "struct depth_vertex_output {{\n"
    "  float2 uv : TEXCOORD;\n"
    "  float4 sv_position : SV_Position;\n"
    "}}; // struct depth_vertex_output\n"
    "\n"
    "struct depth_fragment_input {{\n"
    "  float2 uv : TEXCOORD;\n"
    "  float4 sv_position : SV_Position;\n"
    "}}; // struct depth_fragment_input\n"
    "\n"
    "[shader(\"vertex\")]\n"
    "depth_vertex_output depth_vertex_main(vertex_input input) {{\n"
    "  vertex v = push.vertices[input.vertex_id];\n"
    "  frame_data frame_data = *push.frame_data;\n"
    "\n"
    "  transform_data instance = push.transforms[push.transform_offset + input.instance_id];\n"
    "\n"
    "  float4 world_position = mul(instance.model, float4(graph_vertex_position(v), 1.0));\n"
    "\n"
    "  depth_vertex_output output;\n"
    "  output.uv = v.uv;\n"
    "\n"
    // depth_pre_pass and shadow_pass share this one entry point, but need different transforms --
    // depth_pre_pass is camera-space depth (matches vertex_main above), shadow_pass is this
    // cascade's own light-space view-projection (matches shaders/passes/shadow.slang's own
    // vertex_main exactly). push.cascade_index (render_pass.hpp's push_constants::cascade_index)
    // is the same sentinel-vs-real-index distinction submit_draw_commands already keys its own
    // cascade_index push-constant write on, just read back here instead of written.
    "  if (push.cascade_index == 0xFFFFFFFFu) {{\n"
    "    output.sv_position = mul(frame_data.projection, mul(frame_data.view, world_position));\n"
    "  }} else {{\n"
    "    output.sv_position = mul(frame_data.light_view_projections[push.cascade_index], world_position);\n"
    "  }}\n"
    "\n"
    "  return output;\n"
    "}}\n"
    "\n"
    "[shader(\"fragment\")]\n"
    "void depth_fragment_main(depth_fragment_input input) {{\n"
    "  material_data material = push.frame_data->materials[push.material_index];\n"
    "\n"
    "  float2 uv = apply_uv_transform(material, input.uv);\n"
    "  float3 n = float3(0.0, 0.0, 1.0);\n"
    "  float3 view_dir = float3(0.0, 0.0, 1.0);\n"
    "  float3 world_position = float3(0.0, 0.0, 0.0);\n" // depth_fragment_input carries no real position -- same dummy-fallback reasoning as n/view_dir above
    "\n"
    "{6}"
    "  uint is_alpha_masked = (material.flags & material_flags::alpha_masked) != 0u;\n"
    "  clip(is_alpha_masked * ({7} - {8}));\n"
    "}}\n"
    "\n"
    "[shader(\"fragment\")]\n"
    "Policy::Output fragment_main<Policy : shading_policy>(fragment_input input) {{\n"
    "  lighting_input lighting_input = build_lighting_input(input);\n"
    "  material_data material = push.frame_data->materials[push.material_index];\n"
    "\n"
    "  float2 uv = apply_uv_transform(material, input.uv);\n"
    "  float3 n = normalize(lighting_input.normal);\n"
    "  float3 view_dir = normalize(lighting_input.frame_data.camera_position.xyz - lighting_input.position);\n"
    "  float3 world_position = lighting_input.position;\n"
    "\n"
    "{4}"
    "\n"
    "  shading_input shading_input;\n"
    "  shading_input.lighting = lighting_output;\n"
    "  shading_input.depth = input.sv_position.z;\n"
    "\n"
    "  return Policy::shade(shading_input);\n"
    "}}\n",
    graph_name, *vertex_position_fn, *vertex_normal_fn, *vertex_tangent_fn, fragment_body, noise_helper_source_for(graph),
    depth_fragment_body, depth_alpha, depth_alpha_clip_threshold
  );

  return source;
}

auto generate_shader_graph_preview_source(const std::string& graph_name, const shader_graph::create_info& graph) -> std::expected<std::string, std::string> {
  const auto lit_it = std::ranges::find_if(graph.nodes, [](const shader_graph_node& node) { return node.type == shader_node_type::output_fragment_lit; });
  const auto unlit_it = std::ranges::find_if(graph.nodes, [](const shader_graph_node& node) { return node.type == shader_node_type::output_fragment_unlit; });
  const auto has_lit = lit_it != graph.nodes.end();
  const auto has_unlit = unlit_it != graph.nodes.end();

  if (!has_lit && !has_unlit) {
    return std::unexpected{"graph has no Fragment output -- add a Fragment (Lit) or Fragment (Unlit) node"};
  }

  if (has_lit && has_unlit) {
    return std::unexpected{"graph has both a Fragment (Lit) and a Fragment (Unlit) output -- exactly one is required"};
  }

  if (const auto error = validate_shader_graph_types(graph)) {
    return std::unexpected{*error};
  }

  const auto vertex_it = std::ranges::find_if(graph.nodes, [](const shader_graph_node& node) { return node.type == shader_node_type::output_vertex; });
  const auto* vertex_node = vertex_it != graph.nodes.end() ? &*vertex_it : nullptr;

  // See generate_shader_graph_source's own per-component vertex walkers -- same shape, just the
  // preview's own sampler-index/time/delta-time expressions (see shader_graph_codegen_walker's
  // constructor doc comment) and material declaration (below) instead of the real path's. Time is
  // packed into camera_position's own otherwise-unused .w (same budget-fitting trick as the sampler
  // index); Delta Time is a fixed literal -- see this function's own doc comment for why that's fine
  // here.
  static constexpr auto preview_delta_time_expression = "(1.0 / 60.0)";

  static constexpr auto preview_material_declaration = "  material_data material = *push.material;\n";

  auto vertex_position_walker = shader_graph_codegen_walker{graph, codegen_context::vertex, "uint(push.light_direction.w)", "push.camera_position.w", preview_delta_time_expression};
  const auto vertex_position_fn = labeled(emit_vertex_component(vertex_node, 0u, vertex_position_walker, "graph_vertex_position", "v.position", preview_material_declaration), "Vertex Position");
  if (!vertex_position_fn) return std::unexpected{vertex_position_fn.error()};

  auto vertex_normal_walker = shader_graph_codegen_walker{graph, codegen_context::vertex, "uint(push.light_direction.w)", "push.camera_position.w", preview_delta_time_expression};
  const auto vertex_normal_fn = labeled(emit_vertex_component(vertex_node, 1u, vertex_normal_walker, "graph_vertex_normal", "v.normal", preview_material_declaration), "Vertex Normal");
  if (!vertex_normal_fn) return std::unexpected{vertex_normal_fn.error()};

  auto vertex_tangent_walker = shader_graph_codegen_walker{graph, codegen_context::vertex, "uint(push.light_direction.w)", "push.camera_position.w", preview_delta_time_expression};
  const auto vertex_tangent_fn = labeled(emit_vertex_component(vertex_node, 2u, vertex_tangent_walker, "graph_vertex_tangent", "v.tangent.xyz", preview_material_declaration), "Vertex Tangent");
  if (!vertex_tangent_fn) return std::unexpected{vertex_tangent_fn.error()};

  auto fragment_walker = shader_graph_codegen_walker{graph, codegen_context::fragment, "uint(push.light_direction.w)", "push.camera_position.w", preview_delta_time_expression, "0.5", "float4(0.5, 0.5, 0.0, 1.0)"};
  auto fragment_body = std::string{};

  if (has_lit) {
    const auto id = lit_it->id;

    // ponytail: only Albedo/Normal/Emission feed the preview's shading -- Metallic/Roughness/
    // Occlusion/Alpha are real Fragment (Lit) sockets (still validated as part of the graph) but
    // this function never evaluates them, since doing anything with them needs a BRDF
    // (evaluate_lit_surface), which needs a fully-populated frame_data this preview deliberately
    // doesn't have (see this function's own doc comment). Revisit with a small Blinn-Phong term
    // if the flat-diffuse-only look turns out to matter in practice.
    const auto albedo = labeled(emit_fragment_socket(id, 0u, fragment_walker, "float3", "float3(1.0, 1.0, 1.0)"), "Fragment (Lit) Albedo");
    if (!albedo) return std::unexpected{albedo.error()};

    const auto normal = labeled(emit_fragment_socket(id, 1u, fragment_walker, "float3", "float3(0.0, 0.0, 1.0)"), "Fragment (Lit) Normal");
    if (!normal) return std::unexpected{normal.error()};

    const auto emission = labeled(emit_fragment_socket(id, 4u, fragment_walker, "float3", "float3(0.0, 0.0, 0.0)"), "Fragment (Lit) Emission");
    if (!emission) return std::unexpected{emission.error()};

    fragment_body = join_statements(fragment_walker.statements()) + fmt::format(
      "  float3 tangent_space_normal = {0};\n"
      "  float3 bitangent = cross(n, input.tangent.xyz) * input.tangent.w;\n"
      "  float3 world_normal = normalize(mul(tangent_space_normal, float3x3(input.tangent.xyz, bitangent, n)));\n"
      "\n"
      "  float3 light_dir = normalize(-push.light_direction.xyz);\n"
      "  float n_dot_l = max(dot(world_normal, light_dir), 0.0);\n"
      "\n"
      "  float3 albedo = {1};\n"
      "  float3 color = albedo * 0.15 + albedo * push.light_color.rgb * push.light_color.a * n_dot_l + {2};\n",
      *normal, *albedo, *emission
    );
  } else {
    const auto id = unlit_it->id;

    const auto color = labeled(emit_fragment_socket(id, 0u, fragment_walker, "float3", ""), "Fragment (Unlit) Color");
    if (!color) return std::unexpected{color.error()};

    // ponytail: Alpha isn't evaluated -- the preview always shows a fully opaque sphere (real Alpha
    // Clip/blend behavior is a materials-system concern, not something a shading preview needs).
    fragment_body = join_statements(fragment_walker.statements()) + fmt::format("  float3 color = {0};\n", *color);
  }

  auto source = fmt::format(
    "// AUTO-GENERATED shader graph preview (shader_graph: {0}). Do not hand-edit.\n"
    "#include <frame_data.slang>\n"
    "\n"
    // Declared before the graph_vertex_*/fragment_main below (unlike the real path's push_data,
    // which arrives already-declared via geometry_common.slang's own #include, ahead of anything
    // this file itself emits) -- graph_vertex_position/normal/tangent's own material_data
    // declaration (preview_material_declaration, above) already references `push.material`, so
    // `push_data`/`push` have to exist before those functions do, not after.
    "struct push_data {{\n"
    "  float4x4 model_view_projection;\n"
    "  float4 light_direction; // xyz = direction, w = float(sampler_index)\n"
    "  float4 light_color;     // rgb = color, a = intensity\n"
    "  float4 camera_position; // xyz used; w = time (Delta Time is a fixed literal, see below)\n"
    "  vertex* vertices;\n"
    "  material_data* material;\n"
    "}}; // struct push_data\n"
    "\n"
    "[[vk::push_constant]] ConstantBuffer<push_data> push;\n"
    "\n"
    "{5}"
    "{1}"
    "\n"
    "{2}"
    "\n"
    "{3}"
    "\n"
    "struct vertex_input {{\n"
    "  [[vk::location(0)]] uint vertex_id : SV_VertexID;\n"
    "}}; // struct vertex_input\n"
    "\n"
    "struct vertex_output {{\n"
    "  [[vk::location(0)]] float3 position : POSITION;\n"
    "  [[vk::location(1)]] float3 normal : NORMAL;\n"
    "  [[vk::location(2)]] float2 uv : TEXCOORD;\n"
    "  [[vk::location(3)]] float4 tangent : TANGENT;\n"
    "  float4 sv_position : SV_Position;\n"
    "}}; // struct vertex_output\n"
    "\n"
    "[shader(\"vertex\")]\n"
    "vertex_output vertex_main(vertex_input input) {{\n"
    "  vertex v = push.vertices[input.vertex_id];\n"
    "\n"
    "  float3 object_position = graph_vertex_position(v);\n"
    "  float3 object_normal = graph_vertex_normal(v);\n"
    "  float3 object_tangent = graph_vertex_tangent(v);\n"
    "\n"
    "  vertex_output output;\n"
    "  output.position = object_position;\n" // preview mesh sits at the origin, unscaled -- no model matrix needed
    "  output.normal = normalize(object_normal);\n"
    "  output.tangent = float4(normalize(object_tangent), v.tangent.w);\n"
    "  output.uv = v.uv;\n"
    "  output.sv_position = mul(push.model_view_projection, float4(object_position, 1.0));\n"
    "\n"
    "  return output;\n"
    "}}\n"
    "\n"
    "struct fragment_input {{\n"
    "  [[vk::location(0)]] float3 position : POSITION;\n"
    "  [[vk::location(1)]] float3 normal : NORMAL;\n"
    "  [[vk::location(2)]] float2 uv : TEXCOORD;\n"
    "  [[vk::location(3)]] float4 tangent : TANGENT;\n"
    "  float4 sv_position : SV_Position;\n"
    "}}; // struct fragment_input\n"
    "\n"
    "[shader(\"fragment\")]\n"
    "float4 fragment_main(fragment_input input) : SV_Target0 {{\n"
    "  material_data material = *push.material;\n"
    "\n"
    "  float2 uv = apply_uv_transform(material, input.uv);\n"
    "  float3 n = normalize(input.normal);\n"
    "  float3 view_dir = normalize(push.camera_position.xyz - input.position);\n"
    "  float3 world_position = input.position;\n" // preview mesh sits at the origin unscaled -- object position doubles as "world" position here
    "\n"
    "{4}"
    "\n"
    "  return float4(color, 1.0);\n"
    "}}\n",
    graph_name, *vertex_position_fn, *vertex_normal_fn, *vertex_tangent_fn, fragment_body, noise_helper_source_for(graph)
  );

  return source;
}

auto generate_node_preview_source(const std::string& graph_name, const shader_graph::create_info& graph, std::uint32_t node_id, std::uint32_t output_pin) -> std::expected<std::string, std::string> {
  if (const auto error = validate_shader_graph_types(graph)) {
    return std::unexpected{*error};
  }

  auto walker = shader_graph_codegen_walker{graph, codegen_context::fragment, "uint(push.sampler_index)", "push.time", "push.delta_time", "0.5", "float4(0.5, 0.5, 0.0, 1.0)"};

  const auto value = walker.visit_pin(node_id, output_pin);

  if (!walker.error().empty()) {
    return std::unexpected{walker.error()};
  }

  if (!value) {
    return std::unexpected{fmt::format("node {} pin {} has nothing connected", node_id, output_pin)};
  }

  // Scalar previews as grayscale, vector2 as (x,y,0,1), vector3 as (x,y,z,1), vector4 as-is --
  // matches Unity Shader Graph's own node-preview convention.
  const auto color_expression = [&]() -> std::string {
    if (value->type == "float") return fmt::format("float4({0}, {0}, {0}, 1.0)", value->expression);
    if (value->type == "float2") return fmt::format("float4(({0}).x, ({0}).y, 0.0, 1.0)", value->expression);
    if (value->type == "float3") return fmt::format("float4({0}, 1.0)", value->expression);
    return value->expression; // float4
  }();

  auto source = fmt::format(
    "// AUTO-GENERATED shader graph node preview (shader_graph: {0}, node: {1}, pin: {2}). Do not hand-edit.\n"
    "#include <frame_data.slang>\n"
    "\n"
    "struct push_data {{\n"
    "  material_data* material;\n"
    "  float sampler_index; // small non-negative index, stored as a plain float (see push_data's C++ mirror)\n"
    "  float time;\n"
    "  float delta_time;\n"
    "}}; // struct push_data\n"
    "\n"
    "[[vk::push_constant]] ConstantBuffer<push_data> push;\n"
    "\n"
    "{5}"
    "struct vertex_output {{\n"
    "  [[vk::location(0)]] float2 uv : TEXCOORD;\n"
    "  float4 sv_position : SV_Position;\n"
    "}}; // struct vertex_output\n"
    "\n"
    // Classic single-triangle-covers-the-screen trick -- no vertex buffer/mesh needed at all: for
    // vertex_id 0/1/2, (position.x, position.y) is (0,0)/(2,0)/(0,2), so `position * 2 - 1` puts
    // its 3 corners at (-1,-1)/(3,-1)/(-1,3) -- a triangle that fully covers the [-1,1] NDC square
    // the actual viewport clips down to.
    "[shader(\"vertex\")]\n"
    "vertex_output vertex_main(uint vertex_id : SV_VertexID) {{\n"
    "  float2 position = float2(float((vertex_id << 1) & 2), float(vertex_id & 2));\n"
    "\n"
    "  vertex_output output;\n"
    "  output.uv = position;\n"
    "  output.sv_position = float4(position * 2.0 - 1.0, 0.0, 1.0);\n"
    "\n"
    "  return output;\n"
    "}}\n"
    "\n"
    "struct fragment_input {{\n"
    "  [[vk::location(0)]] float2 uv : TEXCOORD;\n"
    "  float4 sv_position : SV_Position;\n"
    "}}; // struct fragment_input\n"
    "\n"
    "[shader(\"fragment\")]\n"
    "float4 fragment_main(fragment_input input) : SV_Target0 {{\n"
    "  material_data material = *push.material;\n"
    "  float2 uv = input.uv;\n"
    "  float3 n = float3(0.0, 0.0, 1.0);\n"
    "  float3 view_dir = float3(0.0, 0.0, 1.0);\n"
    "\n"
    "{3}"
    "  return {4};\n"
    "}}\n",
    graph_name, node_id, output_pin, join_statements(walker.statements()), color_expression, noise_helper_source_for(graph)
  );

  return source;
}

} // namespace sbx::assets
