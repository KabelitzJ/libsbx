// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/shader_graph_panel.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <unordered_set>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/widgets/text_field.hpp>
#include <editor/widgets/vector_fields.hpp>

#include <editor/panels/inspector_asset_pickers.hpp>

namespace editor {

// Same id-banding reasoning as animation_graph_panel.cpp's own comment -- imgui-node-editor's
// hit-testing collapses NodeId/PinId/LinkId back to a bare pointer value with no per-kind
// scoping, so every id here lives in its own disjoint numeric band. A node can have several input
// pins (unlike animation_graph's one-input/one-output shape), so input pin ids are additionally
// spaced by max_inputs_per_node within their band.
constexpr auto node_band = std::uintptr_t{0};
constexpr auto input_pin_band = std::uintptr_t{1'000'000};
constexpr auto output_pin_band = std::uintptr_t{2'000'000};
constexpr auto link_band = std::uintptr_t{3'000'000};
constexpr auto max_inputs_per_node = std::uintptr_t{4}; // shader_node_input_count never exceeds 3 today -- one spare

static auto node_id_for(std::uint32_t id) -> ax::NodeEditor::NodeId {
  return ax::NodeEditor::NodeId{node_band + static_cast<std::uintptr_t>(id) + 1u};
}

static auto node_id_from_node(ax::NodeEditor::NodeId id) -> std::optional<std::uint32_t> {
  const auto raw = id.Get();

  if (raw <= node_band || raw >= input_pin_band) {
    return std::nullopt;
  }

  return static_cast<std::uint32_t>(raw - node_band - 1u);
}

static auto input_pin_for(std::uint32_t node_id, std::uint32_t pin) -> ax::NodeEditor::PinId {
  return ax::NodeEditor::PinId{input_pin_band + static_cast<std::uintptr_t>(node_id) * max_inputs_per_node + pin + 1u};
}

static auto output_pin_for(std::uint32_t node_id) -> ax::NodeEditor::PinId {
  return ax::NodeEditor::PinId{output_pin_band + static_cast<std::uintptr_t>(node_id) + 1u};
}

struct resolved_pin {
  bool is_output{false};
  std::uint32_t node_id{0u};
  std::uint32_t pin{0u};
}; // struct resolved_pin

static auto resolve_pin(ax::NodeEditor::PinId id) -> std::optional<resolved_pin> {
  const auto raw = id.Get();

  if (raw > output_pin_band && raw < link_band) {
    return resolved_pin{.is_output = true, .node_id = static_cast<std::uint32_t>(raw - output_pin_band - 1u), .pin = 0u};
  }

  if (raw > input_pin_band && raw < output_pin_band) {
    const auto value = raw - input_pin_band - 1u;
    return resolved_pin{.is_output = false, .node_id = static_cast<std::uint32_t>(value / max_inputs_per_node), .pin = static_cast<std::uint32_t>(value % max_inputs_per_node)};
  }

  return std::nullopt;
}

static auto link_id_for(std::size_t edge_index) -> ax::NodeEditor::LinkId {
  return ax::NodeEditor::LinkId{link_band + edge_index + 1u};
}

static auto edge_index_from_link(ax::NodeEditor::LinkId id) -> std::optional<std::size_t> {
  const auto raw = id.Get();

  if (raw <= link_band) {
    return std::nullopt;
  }

  return raw - link_band - 1u;
}

constexpr auto input_pin_color = IM_COL32(94, 174, 255, 255);
constexpr auto output_pin_color = IM_COL32(255, 176, 79, 255);

// Same Unreal-Blueprint-style filled/hollow pin icon as animation_graph_panel.cpp's own
// draw_pin_icon -- duplicated rather than shared since the two panels don't otherwise depend on
// each other and this is a handful of lines.
static auto draw_pin_icon(bool connected, ImU32 color) -> void {
  constexpr auto diameter = 11.0f;

  const auto line_height = ImGui::GetTextLineHeight();

  auto* draw_list = ImGui::GetWindowDrawList();
  const auto cursor = ImGui::GetCursorScreenPos();
  const auto center = ImVec2{cursor.x + diameter * 0.5f, cursor.y + line_height * 0.5f};
  const auto radius = diameter * 0.5f - 1.0f;

  if (connected) {
    draw_list->AddCircleFilled(center, radius, color, 12);
  } else {
    draw_list->AddCircle(center, radius, color, 12, 1.5f);
  }

  ImGui::Dummy(ImVec2{diameter, line_height});
}

// Every shader_node_type, for the Add Node palette -- grouped by shader_node_category_of at draw
// time rather than kept pre-sorted here, so adding a new enumerator to shader_graph.hpp only ever
// needs updating in one place (this list) to appear in the palette too.
constexpr auto all_node_types = std::array<sbx::assets::shader_node_type, 23u>{
  sbx::assets::shader_node_type::input_uv,
  sbx::assets::shader_node_type::input_normal,
  sbx::assets::shader_node_type::input_view_dir,
  sbx::assets::shader_node_type::input_light_dir,
  sbx::assets::shader_node_type::input_light_color,
  sbx::assets::shader_node_type::constant_float,
  sbx::assets::shader_node_type::constant_vector3,
  sbx::assets::shader_node_type::constant_color,
  sbx::assets::shader_node_type::texture_sample,
  sbx::assets::shader_node_type::add,
  sbx::assets::shader_node_type::subtract,
  sbx::assets::shader_node_type::multiply,
  sbx::assets::shader_node_type::divide,
  sbx::assets::shader_node_type::lerp,
  sbx::assets::shader_node_type::dot,
  sbx::assets::shader_node_type::cross,
  sbx::assets::shader_node_type::normalize,
  sbx::assets::shader_node_type::saturate,
  sbx::assets::shader_node_type::pow,
  sbx::assets::shader_node_type::step,
  sbx::assets::shader_node_type::smoothstep,
  sbx::assets::shader_node_type::output_direct,
  sbx::assets::shader_node_type::output_extra,
};

static auto category_name(sbx::assets::shader_node_category category) -> const char* {
  switch (category) {
    case sbx::assets::shader_node_category::input: return ICON_MDI_IMPORT " Input";
    case sbx::assets::shader_node_category::constant: return ICON_MDI_NUMERIC " Constant";
    case sbx::assets::shader_node_category::texture: return ICON_MDI_IMAGE " Texture";
    case sbx::assets::shader_node_category::math: return ICON_MDI_FUNCTION_VARIANT " Math";
    case sbx::assets::shader_node_category::output: return ICON_MDI_EXPORT " Output";
  }

  return "?";
}

// Only one output_direct is meaningful per graph (codegen requires exactly one); output_extra is
// optional but still only makes sense once. Both still show in the palette if the graph doesn't
// already have one -- adding a second is harmless (codegen just uses whichever it finds), but the
// palette hides the redundant choice once satisfied rather than let a graph quietly end up with
// two direct-output roots that'd need reconciling later.
static auto node_type_already_present(const sbx::assets::shader_graph::create_info& edit, sbx::assets::shader_node_type type) -> bool {
  if (type != sbx::assets::shader_node_type::output_direct && type != sbx::assets::shader_node_type::output_extra) {
    return false;
  }

  return std::ranges::any_of(edit.nodes, [type](const auto& node) { return node.type == type; });
}

// Default payload for a freshly created node of this type -- matches shader_graph_node_value's
// alternative order, same reasoning parse_shader_graph_file's per-type dispatch uses.
static auto default_value_for(sbx::assets::shader_node_type type) -> sbx::assets::shader_graph_node_value {
  switch (type) {
    case sbx::assets::shader_node_type::constant_float: return 0.0f;
    case sbx::assets::shader_node_type::constant_vector3: return sbx::math::vector3{0.0f, 0.0f, 0.0f};
    case sbx::assets::shader_node_type::constant_color: return sbx::math::color{1.0f, 1.0f, 1.0f, 1.0f};
    case sbx::assets::shader_node_type::texture_sample: return sbx::assets::texture_handle{};
    default: return std::monostate{};
  }
}

shader_graph_panel::shader_graph_panel() {
  auto config = ax::NodeEditor::Config{};
  config.SettingsFile = nullptr; // node positions round-trip through shader_graph_node::editor_position instead

  _context = ax::NodeEditor::CreateEditor(&config);
}

shader_graph_panel::~shader_graph_panel() {
  ax::NodeEditor::DestroyEditor(_context);
}

auto shader_graph_panel::_open(sbx::assets::shader_graph_handle graph, std::filesystem::path path) -> void {
  _graph = std::move(graph);
  _path = std::move(path);
  _is_open = true;
  _selection = std::monostate{};
  _seeded_positions.clear();

  if (_graph.is_valid()) {
    _edit.name = _graph->name();
    _edit.nodes = _graph->nodes();
    _edit.edges = _graph->edges();
  } else {
    _edit = sbx::assets::shader_graph::create_info{};
  }
}

auto shader_graph_panel::_apply_live() -> void {
  if (!_graph.is_valid()) {
    return;
  }

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  assets_module.update_shader_graph(_graph, _edit);
}

auto shader_graph_panel::_next_node_id() const -> std::uint32_t {
  auto next = std::uint32_t{0u};

  for (const auto& node : _edit.nodes) {
    next = std::max(next, node.id + 1u);
  }

  return next;
}

auto shader_graph_panel::draw(editor_state& state) -> void {
  if (state.open_shader_graph_request.has_value()) {
    const auto request = *state.open_shader_graph_request;
    state.open_shader_graph_request.reset();

    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

    _open(assets_module.load_shader_graph(request.id), request.path);
  }

  if (!_is_open) {
    return;
  }

  const auto title = fmt::format("{} Shader Graph \xe2\x80\x94 {}###shader_graph_panel", ICON_MDI_VECTOR_POLYLINE, _path.stem().string());

  ImGui::SetNextWindowSize(ImVec2{900.0f, 600.0f}, ImGuiCond_FirstUseEver);

  if (!ImGui::Begin(title.c_str(), &_is_open)) {
    ImGui::End();
    return;
  }

  if (!_graph.is_valid()) {
    ImGui::TextDisabled("Could not load this shader graph.");
    ImGui::End();
    return;
  }

  _draw_toolbar();

  ImGui::Separator();

  const auto region = ImGui::GetContentRegionAvail();
  constexpr auto inspector_width = 320.0f;

  ImGui::BeginChild("##shader_graph_canvas_region", ImVec2{region.x - inspector_width, 0.0f}, ImGuiChildFlags_Borders);
  _draw_canvas();
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("##shader_graph_selection_region", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_Borders);
  _draw_selection_inspector();
  ImGui::EndChild();

  ImGui::End();
}

auto shader_graph_panel::_draw_toolbar() -> void {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
    assets_module.save_shader_graph(_graph, _path);
  }

  ImGui::SameLine();

  const auto has_output_direct = std::ranges::any_of(_edit.nodes, [](const auto& node) { return node.type == sbx::assets::shader_node_type::output_direct; });

  if (!has_output_direct) {
    ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f}, ICON_MDI_ALERT " No Direct Output node -- this graph won't compile until one is added and connected.");
  } else {
    ImGui::TextDisabled("Right-click the canvas to add a node.");
  }
}

auto shader_graph_panel::_draw_add_node_menu(sbx::math::vector2 spawn_position) -> void {
  for (const auto category : {sbx::assets::shader_node_category::input, sbx::assets::shader_node_category::constant, sbx::assets::shader_node_category::texture, sbx::assets::shader_node_category::math, sbx::assets::shader_node_category::output}) {
    if (!ImGui::BeginMenu(category_name(category))) {
      continue;
    }

    for (const auto type : all_node_types) {
      if (sbx::assets::shader_node_category_of(type) != category || node_type_already_present(_edit, type)) {
        continue;
      }

      const auto label = std::string{sbx::assets::shader_node_display_name(type)};

      if (ImGui::MenuItem(label.c_str())) {
        auto node = sbx::assets::shader_graph_node{};
        node.id = _next_node_id();
        node.type = type;
        node.editor_position = spawn_position;
        node.value = default_value_for(type);

        _edit.nodes.push_back(node);
        _selection = node.id;
        _apply_live();
      }
    }

    ImGui::EndMenu();
  }
}

auto shader_graph_panel::_draw_canvas() -> void {
  ax::NodeEditor::SetCurrentEditor(_context);
  ax::NodeEditor::Begin("##shader_graph_node_canvas", ImVec2{0.0f, 0.0f});

  // Which input pins currently have an edge attached -- drives draw_pin_icon's filled/hollow look.
  // Every node has at most one output, so an output pin's "connected" state is just "any edge
  // starts here" (a set is still needed since the same output can feed several inputs).
  auto connected_inputs = std::unordered_set<std::uint64_t>{}; // (node_id << 32) | pin
  auto connected_outputs = std::unordered_set<std::uint32_t>{};

  for (const auto& edge : _edit.edges) {
    connected_inputs.insert((static_cast<std::uint64_t>(edge.to_node) << 32u) | edge.to_pin);
    connected_outputs.insert(edge.from_node);
  }

  for (auto& node : _edit.nodes) {
    const auto id = node_id_for(node.id);

    if (!_seeded_positions.contains(node.id)) {
      ax::NodeEditor::SetNodePosition(id, ImVec2{node.editor_position.x(), node.editor_position.y()});
      _seeded_positions.insert(node.id);
    }

    ax::NodeEditor::BeginNode(id);

    ImGui::TextUnformatted(sbx::assets::shader_node_display_name(node.type));

    if (!node.name.empty() && (node.type == sbx::assets::shader_node_type::constant_float || node.type == sbx::assets::shader_node_type::constant_vector3 || node.type == sbx::assets::shader_node_type::constant_color || node.type == sbx::assets::shader_node_type::texture_sample)) {
      ImGui::SameLine();
      ImGui::TextDisabled("(%s)", node.name.c_str());
    }

    if (sbx::assets::shader_node_has_output(node.type)) {
      ImGui::SameLine();
      ax::NodeEditor::BeginPin(output_pin_for(node.id), ax::NodeEditor::PinKind::Output);
      draw_pin_icon(connected_outputs.contains(node.id), output_pin_color);
      ax::NodeEditor::EndPin();
    }

    const auto input_count = sbx::assets::shader_node_input_count(node.type);

    for (auto pin = std::uint32_t{0u}; pin < input_count; ++pin) {
      ax::NodeEditor::BeginPin(input_pin_for(node.id, pin), ax::NodeEditor::PinKind::Input);
      draw_pin_icon(connected_inputs.contains((static_cast<std::uint64_t>(node.id) << 32u) | pin), input_pin_color);
      ax::NodeEditor::EndPin();

      ImGui::SameLine();
      ImGui::TextUnformatted(sbx::assets::shader_node_input_label(node.type, pin));
    }

    ax::NodeEditor::EndNode();

    const auto position = ax::NodeEditor::GetNodePosition(id);

    if (position.x != node.editor_position.x() || position.y != node.editor_position.y()) {
      node.editor_position = sbx::math::vector2{position.x, position.y};
      _apply_live();
    }
  }

  for (auto index = std::size_t{0u}; index < _edit.edges.size(); ++index) {
    const auto& edge = _edit.edges[index];
    ax::NodeEditor::Link(link_id_for(index), output_pin_for(edge.from_node), input_pin_for(edge.to_node, edge.to_pin));
  }

  if (ax::NodeEditor::BeginCreate()) {
    auto start_pin = ax::NodeEditor::PinId{};
    auto end_pin = ax::NodeEditor::PinId{};

    if (ax::NodeEditor::QueryNewLink(&start_pin, &end_pin)) {
      const auto start = resolve_pin(start_pin);
      const auto end = resolve_pin(end_pin);

      const auto source = (start && start->is_output) ? start : ((end && end->is_output) ? end : std::nullopt);
      const auto target = (start && !start->is_output) ? start : ((end && !end->is_output) ? end : std::nullopt);

      if (!source || !target || source->node_id == target->node_id) {
        ax::NodeEditor::RejectNewItem(ImVec4{1.0f, 0.3f, 0.3f, 1.0f});
      } else if (ax::NodeEditor::AcceptNewItem()) {
        // An input pin accepts one connection -- a new drag onto an already-connected pin replaces
        // it, matching Unity Shader Graph's own convention, rather than stacking a second edge onto
        // the same pin (which codegen has no defined meaning for -- see shader_graph_codegen.cpp's
        // _incoming map, keyed by (to_node, to_pin), last-write-wins today for exactly this reason).
        std::erase_if(_edit.edges, [&target](const auto& edge) { return edge.to_node == target->node_id && edge.to_pin == target->pin; });

        _edit.edges.push_back(sbx::assets::shader_graph_edge{.from_node = source->node_id, .from_pin = 0u, .to_node = target->node_id, .to_pin = target->pin});
        _selection = _edit.edges.size() - 1u;
        _apply_live();
      }
    }
  }

  ax::NodeEditor::EndCreate();

  if (ax::NodeEditor::BeginDelete()) {
    auto link_id = ax::NodeEditor::LinkId{};

    while (ax::NodeEditor::QueryDeletedLink(&link_id)) {
      const auto edge_index = edge_index_from_link(link_id);

      if (edge_index.has_value() && ax::NodeEditor::AcceptDeletedItem()) {
        if (*edge_index < _edit.edges.size()) {
          _edit.edges.erase(_edit.edges.begin() + static_cast<std::ptrdiff_t>(*edge_index));
        }

        _selection = std::monostate{};
        _apply_live();
      }
    }

    auto node_id = ax::NodeEditor::NodeId{};

    while (ax::NodeEditor::QueryDeletedNode(&node_id)) {
      const auto deleted_id = node_id_from_node(node_id);

      if (deleted_id.has_value() && ax::NodeEditor::AcceptDeletedItem()) {
        std::erase_if(_edit.nodes, [&deleted_id](const auto& n) { return n.id == *deleted_id; });
        std::erase_if(_edit.edges, [&deleted_id](const auto& e) { return e.from_node == *deleted_id || e.to_node == *deleted_id; });

        _seeded_positions.erase(*deleted_id);
        _selection = std::monostate{};
        _apply_live();
      }
    }
  }

  ax::NodeEditor::EndDelete();

  const auto spawn_position = ImGui::GetMousePos(); // screen-space, captured before Suspend -- same convention animation_graph_panel/imgui-node-editor's blueprints example use

  ax::NodeEditor::Suspend();

  if (ax::NodeEditor::ShowBackgroundContextMenu()) {
    ImGui::OpenPopup("##shader_graph_background_context");
  }

  if (ImGui::BeginPopup("##shader_graph_background_context")) {
    const auto canvas_position = ax::NodeEditor::ScreenToCanvas(spawn_position);
    _draw_add_node_menu(sbx::math::vector2{canvas_position.x, canvas_position.y});

    ImGui::EndPopup();
  }

  ax::NodeEditor::Resume();

  auto selected_node = ax::NodeEditor::NodeId{};
  auto selected_link = ax::NodeEditor::LinkId{};

  if (ax::NodeEditor::GetSelectedNodes(&selected_node, 1) > 0) {
    if (const auto selected_id = node_id_from_node(selected_node)) {
      _selection = *selected_id;
    }
  } else if (ax::NodeEditor::GetSelectedLinks(&selected_link, 1) > 0) {
    if (const auto selected_edge_index = edge_index_from_link(selected_link)) {
      _selection = *selected_edge_index;
    }
  } else {
    _selection = std::monostate{};
  }

  ax::NodeEditor::End();
}

auto shader_graph_panel::_draw_selection_inspector() -> void {
  if (std::holds_alternative<std::uint32_t>(_selection)) {
    const auto node_id = std::get<std::uint32_t>(_selection);
    const auto it = std::ranges::find(_edit.nodes, node_id, &sbx::assets::shader_graph_node::id);

    if (it == _edit.nodes.end()) {
      _selection = std::monostate{};
      return;
    }

    auto& node = *it;

    ImGui::SeparatorText(sbx::assets::shader_node_display_name(node.type));

    const auto is_constant = node.type == sbx::assets::shader_node_type::constant_float || node.type == sbx::assets::shader_node_type::constant_vector3 || node.type == sbx::assets::shader_node_type::constant_color;
    const auto is_texture = node.type == sbx::assets::shader_node_type::texture_sample;

    if (is_constant || is_texture) {
      if (draw_text_field(is_texture ? "Texture Name" : "Parameter Name", node.name)) {
        _apply_live();
      }

      if (is_constant) {
        if (ImGui::Checkbox("Exposed", &node.exposed)) {
          _apply_live();
        }

        ImGui::TextDisabled("Exposed constants become tunable per-material parameters.");
      }
    }

    switch (node.type) {
      case sbx::assets::shader_node_type::constant_float: {
        auto value = std::holds_alternative<std::float_t>(node.value) ? std::get<std::float_t>(node.value) : 0.0f;

        if (ImGui::DragFloat("Value", &value, 0.01f)) {
          node.value = value;
          _apply_live();
        }

        break;
      }
      case sbx::assets::shader_node_type::constant_vector3: {
        auto value = std::holds_alternative<sbx::math::vector3>(node.value) ? std::get<sbx::math::vector3>(node.value) : sbx::math::vector3{};
        auto components = std::array<std::float_t, 3u>{value.x(), value.y(), value.z()};

        if (draw_vector3_control("Value", components, 0.0f, 0.01f).changed) {
          node.value = sbx::math::vector3{components[0], components[1], components[2]};
          _apply_live();
        }

        break;
      }
      case sbx::assets::shader_node_type::constant_color: {
        auto value = std::holds_alternative<sbx::math::color>(node.value) ? std::get<sbx::math::color>(node.value) : sbx::math::color{1.0f, 1.0f, 1.0f, 1.0f};

        if (draw_color_field("Value", value)) {
          node.value = value;
          _apply_live();
        }

        break;
      }
      case sbx::assets::shader_node_type::texture_sample: {
        ImGui::TextDisabled("Texture value is set per-material once this graph is assigned (Material Inspector's Shader Graph section) -- this node only declares the slot and its UV input.");
        break;
      }
      default:
        ImGui::TextDisabled("This node has no editable properties.");
        break;
    }
  } else if (std::holds_alternative<std::size_t>(_selection)) {
    const auto index = std::get<std::size_t>(_selection);

    if (index >= _edit.edges.size()) {
      _selection = std::monostate{};
      return;
    }

    const auto& edge = _edit.edges[index];

    const auto node_label = [this](std::uint32_t id) {
      const auto it = std::ranges::find(_edit.nodes, id, &sbx::assets::shader_graph_node::id);
      return it != _edit.nodes.end() ? std::string{sbx::assets::shader_node_display_name(it->type)} : std::string{"(unknown)"};
    };

    ImGui::SeparatorText("Edge");
    ImGui::Text("From: %s", node_label(edge.from_node).c_str());

    const auto to_it = std::ranges::find(_edit.nodes, edge.to_node, &sbx::assets::shader_graph_node::id);
    const auto to_input_label = (to_it != _edit.nodes.end()) ? std::string{sbx::assets::shader_node_input_label(to_it->type, edge.to_pin)} : std::string{"?"};

    ImGui::Text("To: %s (%s)", node_label(edge.to_node).c_str(), to_input_label.c_str());
  } else {
    ImGui::TextDisabled("Select a node or edge.");
    ImGui::Spacing();
    ImGui::TextDisabled("Right-click the canvas to add a node.");
  }
}

} // namespace editor
