// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/shader_graph_panel.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_set>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/shader_graph_codegen.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/widgets/text_field.hpp>
#include <editor/widgets/vector_fields.hpp>

#include <editor/panels/inspector_asset_pickers.hpp>

namespace editor {

// imgui-node-editor's hit-testing reduces node, pin and link ids to bare pointer values, so each kind gets its own disjoint id band, with fixed strides for multi-pin nodes.
constexpr auto node_band = std::uintptr_t{0};
constexpr auto input_pin_band = std::uintptr_t{1'000'000};
constexpr auto output_pin_band = std::uintptr_t{2'000'000};
constexpr auto link_band = std::uintptr_t{3'000'000};
constexpr auto max_inputs_per_node = std::uintptr_t{8};  // the most today is 7 (Fragment (Lit))
constexpr auto max_outputs_per_node = std::uintptr_t{5}; // the most today is 5 (Sample Texture)

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

static auto output_pin_for(std::uint32_t node_id, std::uint32_t pin) -> ax::NodeEditor::PinId {
  return ax::NodeEditor::PinId{output_pin_band + static_cast<std::uintptr_t>(node_id) * max_outputs_per_node + pin + 1u};
}

struct resolved_pin {
  bool is_output{false};
  std::uint32_t node_id{0u};
  std::uint32_t pin{0u};
}; // struct resolved_pin

static auto resolve_pin(ax::NodeEditor::PinId id) -> std::optional<resolved_pin> {
  const auto raw = id.Get();

  if (raw > output_pin_band && raw < link_band) {
    const auto value = raw - output_pin_band - 1u;
    return resolved_pin{.is_output = true, .node_id = static_cast<std::uint32_t>(value / max_outputs_per_node), .pin = static_cast<std::uint32_t>(value % max_outputs_per_node)};
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

constexpr auto pin_icon_diameter = 11.0f;

// Width of one inline value field on a constant node; also counted in the node's content width so the output column stays flush right.
constexpr auto inline_field_width = 52.0f;

// Width of a node's inline mode button (Scene Depth, Screen Position); counted in the content width like inline_field_width.
constexpr auto mode_trigger_width = 110.0f;

// The inline value editor's width, a third content-width candidate besides the title and pin columns. constant_color returns 0: its fixed swatch is centered instead.
[[nodiscard]] static auto inline_editor_natural_width(sbx::assets::shader_node_type type) -> float {
  switch (type) {
    case sbx::assets::shader_node_type::constant_float: return inline_field_width * 2.0f;
    case sbx::assets::shader_node_type::constant_vector2: return inline_field_width * 2.0f + ImGui::GetStyle().ItemSpacing.x;
    case sbx::assets::shader_node_type::constant_vector3: return inline_field_width * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
    case sbx::assets::shader_node_type::constant_vector4: return inline_field_width * 4.0f + ImGui::GetStyle().ItemSpacing.x * 3.0f;
    case sbx::assets::shader_node_type::scene_depth:
    case sbx::assets::shader_node_type::screen_position:
      return mode_trigger_width;
    default: return 0.0f;
  }
}

// Pins and links are colored by the value type flowing through them, like Unity Shader Graph; unresolved dynamic pins are grey.
static auto pin_color_for(std::optional<sbx::assets::shader_value_type> type) -> ImVec4 {
  if (!type) {
    return ImVec4(0.576f, 0.584f, 0.654f, 1.0f); // overlay2 #9399b2
  }

  switch (*type) {
    case sbx::assets::shader_value_type::scalar:  return ImVec4(0.580f, 0.886f, 0.819f, 1.0f); // teal   #94e2d5
    case sbx::assets::shader_value_type::vector2: return ImVec4(0.650f, 0.890f, 0.631f, 1.0f); // green  #a6e3a1
    case sbx::assets::shader_value_type::vector3: return ImVec4(0.980f, 0.913f, 0.596f, 1.0f); // yellow #f9e2af
    case sbx::assets::shader_value_type::vector4: return ImVec4(0.796f, 0.698f, 0.972f, 1.0f); // mauve  #cba6f7
  }

  return ImVec4(0.576f, 0.584f, 0.654f, 1.0f);
}

// Filled/hollow pin icon, duplicated from animation_graph_panel.cpp since the panels are independent.
static auto draw_pin_icon(bool connected, ImU32 color) -> void {
  const auto line_height = ImGui::GetTextLineHeight();

  auto* draw_list = ImGui::GetWindowDrawList();
  const auto cursor = ImGui::GetCursorScreenPos();
  const auto center = ImVec2{cursor.x + pin_icon_diameter * 0.5f, cursor.y + line_height * 0.5f};
  const auto radius = pin_icon_diameter * 0.5f - 1.0f;

  if (connected) {
    draw_list->AddCircleFilled(center, radius, color, 12);
  } else {
    draw_list->AddCircle(center, radius, color, 12, 1.5f);
  }

  ImGui::Dummy(ImVec2{pin_icon_diameter, line_height});
}

// A pin row's text, e.g. "Albedo(3)", or just the bracket for unlabeled outputs; shared by measuring and drawing so they agree.
static auto pin_row_text(const char* label, std::optional<sbx::assets::shader_value_type> resolved) -> std::string {
  auto text = std::string{label};

  if (resolved) {
    text += fmt::format("({})", sbx::assets::shader_value_type_component_count(*resolved));
  }

  return text;
}

// One row's width (icon plus text), used for column widths and to right-align each output row's own pin.
static auto pin_row_width(const std::string& text) -> float {
  return pin_icon_diameter + (text.empty() ? 0.0f : ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(text.c_str()).x);
}

// The widest output row, in node-local coordinates.
static auto output_column_width(const sbx::assets::shader_graph_node& node, sbx::assets::shader_graph_type_resolver& types) -> float {
  const auto output_count = sbx::assets::shader_node_output_count(node.type);

  auto width = 0.0f;

  for (auto pin = std::uint32_t{0u}; pin < output_count; ++pin) {
    width = std::max(width, pin_row_width(pin_row_text(sbx::assets::shader_node_output_label(node.type, pin), types.output_type(node.id, pin))));
  }

  return width;
}

// The widest input row. Widths come from this frame's measurements only; feeding back last frame's size could grow without bound.
static auto input_column_width(const sbx::assets::shader_graph_node& node, sbx::assets::shader_graph_type_resolver& types) -> float {
  const auto input_count = sbx::assets::shader_node_input_count(node.type);

  auto width = 0.0f;

  for (auto pin = std::uint32_t{0u}; pin < input_count; ++pin) {
    width = std::max(width, pin_row_width(pin_row_text(sbx::assets::shader_node_input_label(node.type, pin), types.input_type(node.id, pin))));
  }

  return width;
}

// Every node type for the Add Node palette, grouped by category at draw time so new types are added here only.
constexpr auto all_node_types = std::array<sbx::assets::shader_node_type, 58u>{
  sbx::assets::shader_node_type::input_uv,
  sbx::assets::shader_node_type::input_normal,
  sbx::assets::shader_node_type::input_view_dir,
  sbx::assets::shader_node_type::input_vertex_position,
  sbx::assets::shader_node_type::input_vertex_normal,
  sbx::assets::shader_node_type::input_vertex_tangent,
  sbx::assets::shader_node_type::camera_position,
  sbx::assets::shader_node_type::main_light_direction,
  sbx::assets::shader_node_type::main_light_color,
  sbx::assets::shader_node_type::time,
  sbx::assets::shader_node_type::delta_time,
  sbx::assets::shader_node_type::scene_depth,
  sbx::assets::shader_node_type::screen_position,
  sbx::assets::shader_node_type::input_world_position,
  sbx::assets::shader_node_type::constant_float,
  sbx::assets::shader_node_type::constant_vector2,
  sbx::assets::shader_node_type::constant_vector3,
  sbx::assets::shader_node_type::constant_vector4,
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
  sbx::assets::shader_node_type::negate,
  sbx::assets::shader_node_type::one_minus,
  sbx::assets::shader_node_type::absolute,
  sbx::assets::shader_node_type::floor,
  sbx::assets::shader_node_type::ceiling,
  sbx::assets::shader_node_type::round,
  sbx::assets::shader_node_type::fraction,
  sbx::assets::shader_node_type::sign,
  sbx::assets::shader_node_type::sine,
  sbx::assets::shader_node_type::cosine,
  sbx::assets::shader_node_type::minimum,
  sbx::assets::shader_node_type::maximum,
  sbx::assets::shader_node_type::clamp,
  sbx::assets::shader_node_type::length,
  sbx::assets::shader_node_type::distance,
  sbx::assets::shader_node_type::reflect,
  sbx::assets::shader_node_type::remap,
  sbx::assets::shader_node_type::fresnel_effect,
  sbx::assets::shader_node_type::simple_noise,
  sbx::assets::shader_node_type::voronoi,
  sbx::assets::shader_node_type::swizzle,
  sbx::assets::shader_node_type::split,
  sbx::assets::shader_node_type::combine,
  sbx::assets::shader_node_type::output_vertex,
  sbx::assets::shader_node_type::output_fragment_lit,
  sbx::assets::shader_node_type::output_fragment_unlit,
};

static auto category_name(sbx::assets::shader_node_category category) -> const char* {
  switch (category) {
    case sbx::assets::shader_node_category::input: return ICON_MDI_IMPORT " Input";
    case sbx::assets::shader_node_category::constant: return ICON_MDI_NUMERIC " Constant";
    case sbx::assets::shader_node_category::texture: return ICON_MDI_IMAGE " Texture";
    case sbx::assets::shader_node_category::basic: return ICON_MDI_FUNCTION_VARIANT " Basic";
    case sbx::assets::shader_node_category::round: return ICON_MDI_CIRCLE_HALF_FULL " Round";
    case sbx::assets::shader_node_category::interpolation: return ICON_MDI_TRANSITION " Interpolation";
    case sbx::assets::shader_node_category::range: return ICON_MDI_ARROW_EXPAND_HORIZONTAL " Range";
    case sbx::assets::shader_node_category::trigonometry: return ICON_MDI_ANGLE_ACUTE " Trigonometry";
    case sbx::assets::shader_node_category::vector: return ICON_MDI_VECTOR_LINE " Vector";
    case sbx::assets::shader_node_category::noise: return ICON_MDI_WAVES " Noise";
    case sbx::assets::shader_node_category::channel: return ICON_MDI_SHUFFLE_VARIANT " Channel";
    case sbx::assets::shader_node_category::output: return ICON_MDI_EXPORT " Output";
  }

  return "?";
}

// Each output sink may appear once per graph, and Fragment (Lit)/(Unlit) exclude each other, so the palette hides those once present.
static auto node_type_already_present(const sbx::assets::shader_graph::create_info& edit, sbx::assets::shader_node_type type) -> bool {
  if (!sbx::assets::shader_node_has_output(type)) {
    if (type == sbx::assets::shader_node_type::output_vertex) {
      return std::ranges::any_of(edit.nodes, [](const auto& node) { return node.type == sbx::assets::shader_node_type::output_vertex; });
    }

    return std::ranges::any_of(edit.nodes, [](const auto& node) { return sbx::assets::shader_node_is_fragment_output(node.type); });
  }

  return false;
}

// A new node's default payload.
static auto default_value_for(sbx::assets::shader_node_type type) -> sbx::assets::shader_graph_node_value {
  switch (type) {
    case sbx::assets::shader_node_type::constant_float: return 0.0f;
    case sbx::assets::shader_node_type::constant_vector2: return sbx::math::vector2{0.0f, 0.0f};
    case sbx::assets::shader_node_type::constant_vector3: return sbx::math::vector3{0.0f, 0.0f, 0.0f};
    case sbx::assets::shader_node_type::constant_vector4: return sbx::math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
    case sbx::assets::shader_node_type::constant_color: return sbx::math::color{1.0f, 1.0f, 1.0f, 1.0f};
    case sbx::assets::shader_node_type::texture_sample: return sbx::assets::texture2d_handle{};
    case sbx::assets::shader_node_type::swizzle: return std::string{"rgba"}; // identity
    case sbx::assets::shader_node_type::scene_depth: return std::string{"linear01"}; // Unity Shader Graph's default
    case sbx::assets::shader_node_type::screen_position: return std::string{"default"}; // Unity Shader Graph's default
    default: return std::monostate{};
  }
}

shader_graph_panel::shader_graph_panel() {
  auto config = ax::NodeEditor::Config{};
  config.SettingsFile = nullptr; // node positions are stored in shader_graph_node::editor_position

  _context = ax::NodeEditor::CreateEditor(&config);

  // Retint imgui-node-editor's default palette to the editor's Catppuccin Mocha theme.
  ax::NodeEditor::SetCurrentEditor(_context);
  auto& style = ax::NodeEditor::GetStyle();
  style.Colors[ax::NodeEditor::StyleColor_Bg]                  = ImColor(0.109f, 0.109f, 0.156f, 1.0f); // mantle #181825
  style.Colors[ax::NodeEditor::StyleColor_Grid]                = ImColor(0.247f, 0.254f, 0.337f, 0.35f); // surface1 #3f4056
  style.Colors[ax::NodeEditor::StyleColor_NodeBg]              = ImColor(0.200f, 0.207f, 0.286f, 0.94f); // surface0 #313244
  style.Colors[ax::NodeEditor::StyleColor_NodeBorder]          = ImColor(0.290f, 0.301f, 0.388f, 1.0f); // surface2 #4a4d63
  style.Colors[ax::NodeEditor::StyleColor_HovNodeBorder]       = ImColor(0.458f, 0.784f, 0.878f, 1.0f); // sapphire #74c7ec
  style.Colors[ax::NodeEditor::StyleColor_SelNodeBorder]       = ImColor(0.796f, 0.698f, 0.972f, 1.0f); // mauve #cba6f7
  style.Colors[ax::NodeEditor::StyleColor_NodeSelRect]         = ImColor(0.796f, 0.698f, 0.972f, 0.15f);
  style.Colors[ax::NodeEditor::StyleColor_NodeSelRectBorder]   = ImColor(0.796f, 0.698f, 0.972f, 0.60f);
  style.Colors[ax::NodeEditor::StyleColor_HovLinkBorder]       = ImColor(0.458f, 0.784f, 0.878f, 1.0f); // sapphire
  style.Colors[ax::NodeEditor::StyleColor_SelLinkBorder]       = ImColor(0.796f, 0.698f, 0.972f, 1.0f); // mauve
  style.Colors[ax::NodeEditor::StyleColor_HighlightLinkBorder] = ImColor(0.980f, 0.709f, 0.572f, 1.0f); // peach #fab387
  style.Colors[ax::NodeEditor::StyleColor_LinkSelRect]         = ImColor(0.796f, 0.698f, 0.972f, 0.15f);
  style.Colors[ax::NodeEditor::StyleColor_LinkSelRectBorder]   = ImColor(0.796f, 0.698f, 0.972f, 0.60f);
  style.Colors[ax::NodeEditor::StyleColor_PinRect]             = ImColor(0.533f, 0.698f, 0.976f, 0.40f); // blue #89b4fa
  style.Colors[ax::NodeEditor::StyleColor_PinRectBorder]       = ImColor(0.533f, 0.698f, 0.976f, 0.60f);
  style.Colors[ax::NodeEditor::StyleColor_Flow]                = ImColor(0.980f, 0.709f, 0.572f, 1.0f); // peach
  style.Colors[ax::NodeEditor::StyleColor_FlowMarker]          = ImColor(0.980f, 0.709f, 0.572f, 1.0f);
  style.Colors[ax::NodeEditor::StyleColor_GroupBg]             = ImColor(0.109f, 0.109f, 0.156f, 0.60f); // mantle
  style.Colors[ax::NodeEditor::StyleColor_GroupBorder]         = ImColor(0.247f, 0.254f, 0.337f, 1.0f); // surface1
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

  // Node ids are graph-local, so previews from the previous graph would collide.
  _node_previews.clear();

  if (_graph.is_valid()) {
    _edit.name = _graph->name();
    _edit.nodes = _graph->nodes();
    _edit.edges = _graph->edges();
  } else {
    _edit = sbx::assets::shader_graph::create_info{};
  }

  _apply_live();
}

auto shader_graph_panel::_apply_live(bool structural) -> void {
  if (!_graph.is_valid()) {
    return;
  }

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  assets_module.update_shader_graph_data(_graph, _edit);

  // Validates with the real codegen; it's pure and cheap, unlike cooking, which only Save does.
  const auto result = sbx::assets::generate_shader_graph_source("shader_graph_preview", _edit);
  _validation_error = result ? std::string{} : result.error();

  // Only structural changes recompile the previews; value changes reach them through the material buffer.
  if (structural && result) {
    _preview.request_recompile(_edit);

    for (const auto& node : _edit.nodes) {
      if (node.preview) {
        _node_previews.request_recompile(_edit, node.id);
      }
    }
  }
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

  ImGui::BeginChild("##shader_graph_right_column", ImVec2{0.0f, 0.0f});

  ImGui::BeginChild("##shader_graph_preview_region", ImVec2{0.0f, 220.0f}, ImGuiChildFlags_Borders);
  _draw_master_preview();
  ImGui::EndChild();

  ImGui::BeginChild("##shader_graph_selection_region", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_Borders);
  _draw_selection_inspector(state);
  ImGui::EndChild();

  ImGui::EndChild();

  ImGui::End();
}

auto shader_graph_panel::_draw_toolbar() -> void {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
    assets_module.save_shader_graph(_graph, _path);
  }

  ImGui::SameLine();

  if (!_validation_error.empty()) {
    ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f}, ICON_MDI_ALERT " %s", _validation_error.c_str());
  } else {
    ImGui::TextDisabled("Right-click the canvas to add a node.");
  }
}

auto shader_graph_panel::_spawn_node(sbx::assets::shader_node_type type, sbx::math::vector2 position) -> std::uint32_t {
  auto node = sbx::assets::shader_graph_node{};
  node.id = _next_node_id();
  node.type = type;
  node.editor_position = position;
  node.value = default_value_for(type);

  _edit.nodes.push_back(node);

  return node.id;
}

auto shader_graph_panel::_add_node(sbx::assets::shader_node_type type, sbx::math::vector2 spawn_position) -> void {
  const auto id = _spawn_node(type, spawn_position);

  // Nodes whose useful wiring would otherwise be an invisible codegen default get that wiring spawned as real, editable nodes.
  static constexpr auto offset_x = 220.0f;

  if (type == sbx::assets::shader_node_type::texture_sample) {
    const auto uv_id = _spawn_node(sbx::assets::shader_node_type::input_uv, spawn_position - sbx::math::vector2{offset_x, 0.0f});
    _edit.edges.push_back(sbx::assets::shader_graph_edge{.from_node = uv_id, .from_pin = 0u, .to_node = id, .to_pin = 0u});
  } else if (type == sbx::assets::shader_node_type::output_vertex) {
    const auto position_id = _spawn_node(sbx::assets::shader_node_type::input_vertex_position, spawn_position - sbx::math::vector2{offset_x, 80.0f});
    const auto normal_id = _spawn_node(sbx::assets::shader_node_type::input_vertex_normal, spawn_position - sbx::math::vector2{offset_x, 0.0f});
    const auto tangent_id = _spawn_node(sbx::assets::shader_node_type::input_vertex_tangent, spawn_position - sbx::math::vector2{offset_x, -80.0f});

    _edit.edges.push_back(sbx::assets::shader_graph_edge{.from_node = position_id, .from_pin = 0u, .to_node = id, .to_pin = 0u});
    _edit.edges.push_back(sbx::assets::shader_graph_edge{.from_node = normal_id, .from_pin = 0u, .to_node = id, .to_pin = 1u});
    _edit.edges.push_back(sbx::assets::shader_graph_edge{.from_node = tangent_id, .from_pin = 0u, .to_node = id, .to_pin = 2u});
  }

  _selection = id;
  _apply_live();
}

auto shader_graph_panel::_draw_add_node_menu(sbx::math::vector2 spawn_position) -> void {
  ImGui::SetNextItemWidth(240.0f);

  if (_add_node_search_focus_pending) {
    ImGui::SetKeyboardFocusHere();
    _add_node_search_focus_pending = false;
  }

  draw_text_field_with_hint("##shader_graph_node_search", ICON_MDI_MAGNIFY " Search nodes...", _add_node_search);

  // A search flattens the categorized submenus into one filtered list.
  if (!_add_node_search.empty()) {
    auto query = _add_node_search;
    std::ranges::transform(query, query.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    ImGui::Separator();

    for (const auto type : all_node_types) {
      if (node_type_already_present(_edit, type)) {
        continue;
      }

      auto label = std::string{sbx::assets::shader_node_display_name(type)};
      auto lowered = label;
      std::ranges::transform(lowered, lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

      if (lowered.find(query) == std::string::npos) {
        continue;
      }

      if (ImGui::MenuItem(label.c_str())) {
        _add_node(type, spawn_position);
      }
    }

    return;
  }

  for (const auto category : {
    sbx::assets::shader_node_category::input,
    sbx::assets::shader_node_category::constant,
    sbx::assets::shader_node_category::texture,
    sbx::assets::shader_node_category::basic,
    sbx::assets::shader_node_category::round,
    sbx::assets::shader_node_category::interpolation,
    sbx::assets::shader_node_category::range,
    sbx::assets::shader_node_category::trigonometry,
    sbx::assets::shader_node_category::vector,
    sbx::assets::shader_node_category::noise,
    sbx::assets::shader_node_category::channel,
    sbx::assets::shader_node_category::output
  }) {
    if (!ImGui::BeginMenu(category_name(category))) {
      continue;
    }

    for (const auto type : all_node_types) {
      if (sbx::assets::shader_node_category_of(type) != category || node_type_already_present(_edit, type)) {
        continue;
      }

      const auto label = std::string{sbx::assets::shader_node_display_name(type)};

      if (ImGui::MenuItem(label.c_str())) {
        _add_node(type, spawn_position);
      }
    }

    ImGui::EndMenu();
  }
}

auto shader_graph_panel::_draw_inline_mode_trigger(const sbx::assets::shader_graph_node& node, const std::string& current_mode, const std::vector<std::pair<std::string, std::string>>& options, float x, float width, bool& popup_requested) -> void {
  auto current_label = options.empty() ? "" : options.front().first.c_str();

  for (const auto& [label, value] : options) {
    if (current_mode == value) {
      current_label = label.c_str();
      break;
    }
  }

  ImGui::SetCursorPosX(x);

  // A Button, not a Combo: Combo's internal popup lands at the wrong position inside a zoomed node. The mode list is drawn after the node loop.
  if (ImGui::Button(current_label, ImVec2{width, 0.0f})) {
    _mode_popup_node_id = node.id;
    _mode_popup_options = options;
    popup_requested = true;
  }
}

auto shader_graph_panel::_draw_canvas() -> void {
  ax::NodeEditor::SetCurrentEditor(_context);
  ax::NodeEditor::Begin("##shader_graph_node_canvas", ImVec2{0.0f, 0.0f});

  // Connected pins, keyed by (node_id << 32 | pin), for the filled/hollow icons.
  auto connected_inputs = std::unordered_set<std::uint64_t>{};
  auto connected_outputs = std::unordered_set<std::uint64_t>{};

  for (const auto& edge : _edit.edges) {
    connected_inputs.insert((static_cast<std::uint64_t>(edge.to_node) << 32u) | edge.to_pin);
    connected_outputs.insert((static_cast<std::uint64_t>(edge.from_node) << 32u) | edge.from_pin);
  }

  auto types = sbx::assets::shader_graph_type_resolver{_edit.nodes, _edit.edges};

  // Set on the frame a color or mode trigger is clicked; the popup is opened after the node loop (see _color_popup_node_id).
  auto color_popup_requested = false;
  auto mode_popup_requested = false;

  for (auto& node : _edit.nodes) {
    const auto id = node_id_for(node.id);

    if (!_seeded_positions.contains(node.id)) {
      ax::NodeEditor::SetNodePosition(id, ImVec2{node.editor_position.x(), node.editor_position.y()});
      _seeded_positions.insert(node.id);
    }

    ax::NodeEditor::BeginNode(id);

    // All nodes share one ImGui window, so cursor X is window-relative; this is the node's left edge in that space.
    const auto content_start_x = ImGui::GetCursorPosX();

    ImGui::TextUnformatted(sbx::assets::shader_node_display_name(node.type));

    const auto output_count = sbx::assets::shader_node_output_count(node.type);
    const auto input_count = sbx::assets::shader_node_input_count(node.type);

    if (!node.name.empty() && (node.type == sbx::assets::shader_node_type::constant_float || node.type == sbx::assets::shader_node_type::constant_vector2 || node.type == sbx::assets::shader_node_type::constant_vector3 || node.type == sbx::assets::shader_node_type::constant_vector4 || node.type == sbx::assets::shader_node_type::constant_color || node.type == sbx::assets::shader_node_type::texture_sample)) {
      ImGui::SameLine();
      ImGui::TextDisabled("(%s)", node.name.c_str());
    } else if (node.type == sbx::assets::shader_node_type::swizzle) {
      // Always shown, since the pattern is the node's whole purpose.
      ImGui::SameLine();
      ImGui::TextDisabled(".%s", sbx::assets::shader_node_swizzle_pattern(node).c_str());
    }

    // The node's content width, set in the pin section and used to center the preview swatch.
    auto node_content_width = 0.0f;

    if (input_count > 0u || output_count > 0u) {
      // Where the title row ends, a content-width candidate measured fresh this frame.
      // The extra SameLine() makes GetCursorPosX() report the finished row's trailing edge instead of the next row's start.
      ImGui::SameLine();
      const auto title_row_end_x = ImGui::GetCursorPosX();

      ImGui::Spacing();

      // Inputs hug the left edge and outputs the right, centered against each other when their row counts differ.
      // Every row is positioned explicitly with SetCursorPos; BeginGroup/SameLine compressed multi-row output columns into each other.
      const auto row_height = ImGui::GetTextLineHeightWithSpacing();
      const auto in_width = input_column_width(node, types);
      const auto out_width = output_column_width(node, types);
      constexpr auto column_gap = 48.0f;
      const auto natural_width = in_width + ((input_count > 0u && output_count > 0u) ? column_gap : 0.0f) + out_width;
      // The widest of the title row, the pin columns and the inline editor, in window-relative X, so outputs stay flush right.
      const auto content_width = std::max({title_row_end_x, content_start_x + natural_width, content_start_x + inline_editor_natural_width(node.type)});
      node_content_width = content_width - content_start_x;

      const auto pins_top = ImGui::GetCursorPosY();

      if (input_count > 0u) {
        auto row_y = pins_top + (output_count > input_count ? static_cast<float>(output_count - input_count) * row_height * 0.5f : 0.0f);

        for (auto pin = std::uint32_t{0u}; pin < input_count; ++pin) {
          ImGui::SetCursorPos(ImVec2{content_start_x, row_y});

          const auto pin_color = ImGui::ColorConvertFloat4ToU32(pin_color_for(types.input_type(node.id, pin)));

          ax::NodeEditor::BeginPin(input_pin_for(node.id, pin), ax::NodeEditor::PinKind::Input);
          draw_pin_icon(connected_inputs.contains((static_cast<std::uint64_t>(node.id) << 32u) | pin), pin_color);
          ax::NodeEditor::EndPin();

          // Width bracket directly against the label, like Unity Shader Graph.
          ImGui::SameLine();
          ImGui::TextUnformatted(pin_row_text(sbx::assets::shader_node_input_label(node.type, pin), types.input_type(node.id, pin)).c_str());

          row_y += row_height;
        }
      }

      if (output_count > 0u) {
        auto row_y = pins_top + (input_count > output_count ? static_cast<float>(input_count - output_count) * row_height * 0.5f : 0.0f);

        for (auto pin = std::uint32_t{0u}; pin < output_count; ++pin) {
          // Right-aligned per row, since multi-output rows have different widths.
          const auto text = pin_row_text(sbx::assets::shader_node_output_label(node.type, pin), types.output_type(node.id, pin));
          ImGui::SetCursorPos(ImVec2{content_width - pin_row_width(text), row_y});

          if (!text.empty()) {
            ImGui::TextUnformatted(text.c_str());
            ImGui::SameLine();
          }

          const auto pin_color = ImGui::ColorConvertFloat4ToU32(pin_color_for(types.output_type(node.id, pin)));

          ax::NodeEditor::BeginPin(output_pin_for(node.id, pin), ax::NodeEditor::PinKind::Output);
          draw_pin_icon(connected_outputs.contains((static_cast<std::uint64_t>(node.id) << 32u) | pin), pin_color);
          ax::NodeEditor::EndPin();

          row_y += row_height;
        }
      }
    }

    // Constants are edited right on the node, like Unity Shader Graph, with compact fixed-width fields since the side-panel widgets would overflow it. PushID keeps field ids unique per node.
    ImGui::PushID(static_cast<std::int32_t>(node.id));

    switch (node.type) {
      case sbx::assets::shader_node_type::constant_float: {
        auto value = std::holds_alternative<std::float_t>(node.value) ? std::get<std::float_t>(node.value) : 0.0f;

        ImGui::SetCursorPosX(content_start_x);
        ImGui::SetNextItemWidth(inline_field_width * 2.0f);

        if (ImGui::DragFloat("##value", &value, 0.01f)) {
          node.value = value;
          _apply_live(!node.exposed);
        }

        break;
      }
      case sbx::assets::shader_node_type::constant_vector2:
      case sbx::assets::shader_node_type::constant_vector3:
      case sbx::assets::shader_node_type::constant_vector4: {
        const auto width = node.type == sbx::assets::shader_node_type::constant_vector2 ? 2u : node.type == sbx::assets::shader_node_type::constant_vector3 ? 3u : 4u;
        static constexpr auto axis_labels = std::array<const char*, 4u>{"##x", "##y", "##z", "##w"};

        auto components = std::array<std::float_t, 4u>{0.0f, 0.0f, 0.0f, 0.0f};

        if (node.type == sbx::assets::shader_node_type::constant_vector2) {
          const auto value = std::holds_alternative<sbx::math::vector2>(node.value) ? std::get<sbx::math::vector2>(node.value) : sbx::math::vector2{};
          components[0] = value.x();
          components[1] = value.y();
        } else if (node.type == sbx::assets::shader_node_type::constant_vector3) {
          const auto value = std::holds_alternative<sbx::math::vector3>(node.value) ? std::get<sbx::math::vector3>(node.value) : sbx::math::vector3{};
          components[0] = value.x();
          components[1] = value.y();
          components[2] = value.z();
        } else {
          const auto value = std::holds_alternative<sbx::math::vector4>(node.value) ? std::get<sbx::math::vector4>(node.value) : sbx::math::vector4{};
          components[0] = value.x();
          components[1] = value.y();
          components[2] = value.z();
          components[3] = value.w();
        }

        ImGui::SetCursorPosX(content_start_x);

        auto changed = false;

        for (auto axis = std::uint32_t{0u}; axis < width; ++axis) {
          if (axis != 0u) {
            ImGui::SameLine();
          }

          ImGui::SetNextItemWidth(inline_field_width);
          changed |= ImGui::DragFloat(axis_labels[axis], &components[axis], 0.01f);
        }

        if (changed) {
          if (node.type == sbx::assets::shader_node_type::constant_vector2) node.value = sbx::math::vector2{components[0], components[1]};
          else if (node.type == sbx::assets::shader_node_type::constant_vector3) node.value = sbx::math::vector3{components[0], components[1], components[2]};
          else node.value = sbx::math::vector4{components[0], components[1], components[2], components[3]};

          _apply_live(!node.exposed);
        }

        break;
      }
      case sbx::assets::shader_node_type::constant_color: {
        auto value = std::holds_alternative<sbx::math::color>(node.value) ? std::get<sbx::math::color>(node.value) : sbx::math::color{1.0f, 1.0f, 1.0f, 1.0f};

        // A fixed-size swatch, centered under the node like the preview image.
        const auto swatch_width = ImGui::GetFrameHeight();
        ImGui::SetCursorPosX(content_start_x + std::max(0.0f, (node_content_width - swatch_width) * 0.5f));

        // A ColorButton only reports the click; the picker popup is drawn after the node loop.
        if (ImGui::ColorButton("##value_swatch", ImVec4{value.r(), value.g(), value.b(), value.a()}, ImGuiColorEditFlags_None, ImVec2{swatch_width, swatch_width})) {
          _color_popup_node_id = node.id;
          color_popup_requested = true;
        }

        break;
      }
      case sbx::assets::shader_node_type::scene_depth: {
        static const auto options = std::vector<std::pair<std::string, std::string>>{{"Raw", "raw"}, {"Eye", "eye"}, {"Linear01", "linear01"}};
        _draw_inline_mode_trigger(node, sbx::assets::shader_node_scene_depth_mode(node), options, content_start_x, mode_trigger_width, mode_popup_requested);
        break;
      }
      case sbx::assets::shader_node_type::screen_position: {
        static const auto options = std::vector<std::pair<std::string, std::string>>{{"Default", "default"}, {"Raw", "raw"}};
        _draw_inline_mode_trigger(node, sbx::assets::shader_node_screen_position_mode(node), options, content_start_x, mode_trigger_width, mode_popup_requested);
        break;
      }
      default:
        break;
    }

    ImGui::PopID();

    if (node.preview) {
      // ImGui's cursor tracking already covers the absolutely positioned pin rows; a full row of gap keeps the swatch from crowding them.
      ImGui::SetCursorPosX(content_start_x);
      ImGui::Dummy(ImVec2{0.0f, ImGui::GetTextLineHeightWithSpacing() * 0.5f});

      constexpr auto swatch_size = ImVec2{64.0f, 64.0f};

      // Centered under the node's content width.
      const auto swatch_x = content_start_x + std::max(0.0f, (node_content_width - swatch_size.x) * 0.5f);
      ImGui::SetCursorPosX(swatch_x);

      if (const auto texture_id = _node_previews.texture_id(node.id)) {
        ImGui::Image(*texture_id, swatch_size);
      } else {
        ImGui::Dummy(swatch_size);
      }
    }

    ax::NodeEditor::EndNode();

    const auto position = ax::NodeEditor::GetNodePosition(id);

    if (position.x != node.editor_position.x() || position.y != node.editor_position.y()) {
      node.editor_position = sbx::math::vector2{position.x, position.y};

      // Not _apply_live(): positions don't affect codegen, and re-cooking every drag frame would stutter.
      if (_graph.is_valid()) {
        auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
        assets_module.update_shader_graph_node_position(_graph, node.id, node.editor_position);
      }
    }
  }

  for (auto index = std::size_t{0u}; index < _edit.edges.size(); ++index) {
    const auto& edge = _edit.edges[index];
    ax::NodeEditor::Link(link_id_for(index), output_pin_for(edge.from_node, edge.from_pin), input_pin_for(edge.to_node, edge.to_pin), pin_color_for(types.output_type(edge.from_node, edge.from_pin)));
  }

  if (ax::NodeEditor::BeginCreate()) {
    auto start_pin = ax::NodeEditor::PinId{};
    auto end_pin = ax::NodeEditor::PinId{};

    if (ax::NodeEditor::QueryNewLink(&start_pin, &end_pin)) {
      const auto start = resolve_pin(start_pin);
      const auto end = resolve_pin(end_pin);

      const auto source = (start && start->is_output) ? start : ((end && end->is_output) ? end : std::nullopt);
      const auto target = (start && !start->is_output) ? start : ((end && !end->is_output) ? end : std::nullopt);

      // Strict typing: fixed pins need an exact match; dynamic pins match the node's resolved type or accept a scalar.
      const auto type_ok = source && target && types.accepts(target->node_id, target->pin, types.output_type(source->node_id, source->pin));

      if (!source || !target || source->node_id == target->node_id || !type_ok) {
        ax::NodeEditor::RejectNewItem(ImVec4{1.0f, 0.3f, 0.3f, 1.0f});
      } else if (ax::NodeEditor::AcceptNewItem()) {
        // An input pin takes one connection; a new one replaces the old, like Unity Shader Graph.
        std::erase_if(_edit.edges, [&target](const auto& edge) { return edge.to_node == target->node_id && edge.to_pin == target->pin; });

        _edit.edges.push_back(sbx::assets::shader_graph_edge{.from_node = source->node_id, .from_pin = source->pin, .to_node = target->node_id, .to_pin = target->pin});
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
        _node_previews.forget(*deleted_id);
        _selection = std::monostate{};
        _apply_live();
      }
    }
  }

  ax::NodeEditor::EndDelete();

  ax::NodeEditor::Suspend();

  // ShowNodeContextMenu only reports the node on the right-click frame, but the popup stays open for many frames, so the id is kept in a member.
  if (auto context_node_id = ax::NodeEditor::NodeId{}; ax::NodeEditor::ShowNodeContextMenu(&context_node_id)) {
    _context_node_id = context_node_id;
    ImGui::OpenPopup("##shader_graph_node_context");
  } else if (ax::NodeEditor::ShowBackgroundContextMenu()) {
    ImGui::OpenPopup("##shader_graph_background_context");
    _add_node_search.clear();
    _add_node_search_focus_pending = true;

    // Captured when the menu opens, so the node spawns where the user right-clicked, not where the mouse wanders in the popup.
    const auto canvas_position = ax::NodeEditor::ScreenToCanvas(ImGui::GetMousePos());
    _add_node_spawn_position = sbx::math::vector2{canvas_position.x, canvas_position.y};
  }

  if (ImGui::BeginPopup("##shader_graph_node_context")) {
    if (const auto context_id = node_id_from_node(_context_node_id)) {
      const auto entry = std::ranges::find(_edit.nodes, *context_id, &sbx::assets::shader_graph_node::id);

      if (entry != _edit.nodes.end()) {
        if (ImGui::MenuItem(entry->preview ? ICON_MDI_EYE_OFF " Hide Preview" : ICON_MDI_EYE " Show Preview")) {
          entry->preview = !entry->preview;

          // Persisted like a position drag, without a recompile: it only affects this node's swatch.
          auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
          assets_module.update_shader_graph_data(_graph, _edit);

          if (entry->preview) {
            _node_previews.request_recompile(_edit, entry->id);
          } else {
            _node_previews.forget(entry->id);
          }
        }

        ImGui::Separator();
      }
    }

    if (ImGui::MenuItem(ICON_MDI_TRASH_CAN " Delete Node")) {
      ax::NodeEditor::DeleteNode(_context_node_id); // reconciled by BeginDelete/QueryDeletedNode next frame
    }

    ImGui::EndPopup();
  }

  if (ImGui::BeginPopup("##shader_graph_background_context")) {
    _draw_add_node_menu(_add_node_spawn_position);

    ImGui::EndPopup();
  }

  // Deferred popups (see _color_popup_node_id): opened only on the click frame, then BeginPopup every frame while they stay open.
  if (color_popup_requested) {
    ImGui::OpenPopup("##shader_graph_color_popup");
  }

  if (ImGui::BeginPopup("##shader_graph_color_popup")) {
    if (_color_popup_node_id) {
      const auto entry = std::ranges::find(_edit.nodes, *_color_popup_node_id, &sbx::assets::shader_graph_node::id);

      if (entry != _edit.nodes.end()) {
        auto value = std::holds_alternative<sbx::math::color>(entry->value) ? std::get<sbx::math::color>(entry->value) : sbx::math::color{1.0f, 1.0f, 1.0f, 1.0f};
        auto components = std::array<std::float_t, 4u>{value.r(), value.g(), value.b(), value.a()};

        if (ImGui::ColorPicker4("##value", components.data())) {
          entry->value = sbx::math::color{components[0], components[1], components[2], components[3]};
          _apply_live(!entry->exposed);
        }
      }
    }

    ImGui::EndPopup();
  }

  if (mode_popup_requested) {
    ImGui::OpenPopup("##shader_graph_mode_popup");
  }

  if (ImGui::BeginPopup("##shader_graph_mode_popup")) {
    if (_mode_popup_node_id) {
      const auto entry = std::ranges::find(_edit.nodes, *_mode_popup_node_id, &sbx::assets::shader_graph_node::id);

      if (entry != _edit.nodes.end()) {
        const auto current_mode = std::holds_alternative<std::string>(entry->value) ? std::get<std::string>(entry->value) : std::string{};

        for (const auto& [label, value] : _mode_popup_options) {
          if (ImGui::Selectable(label.c_str(), current_mode == value)) {
            entry->value = value;
            _apply_live(); // the mode is baked into the generated Slang, so this recompiles
            ImGui::CloseCurrentPopup();
          }
        }
      }
    }

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

auto shader_graph_panel::_draw_master_preview() -> void {
  ImGui::SeparatorText(ICON_MDI_EYE " Preview");

  const auto texture_id = _preview.update(_edit);

  // Node previews share the same material buffer, so value edits update them live too.
  _node_previews.update(_preview.material_address(), _preview.sampler_index(), _preview.material_changed_last_update());

  const auto preview_size = ImVec2{180.0f, 180.0f};

  if (texture_id) {
    ImGui::Image(*texture_id, preview_size);
  } else {
    // Nothing compiled yet; reserve the space so the layout doesn't jump.
    ImGui::Dummy(preview_size);
  }

  if (!_preview.error().empty()) {
    ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f}, ICON_MDI_ALERT " %s", _preview.error().c_str());
  }
}

auto shader_graph_panel::_draw_selection_inspector(editor_state& state) -> void {
  if (std::holds_alternative<std::uint32_t>(_selection)) {
    const auto node_id = std::get<std::uint32_t>(_selection);
    const auto entry = std::ranges::find(_edit.nodes, node_id, &sbx::assets::shader_graph_node::id);

    if (entry == _edit.nodes.end()) {
      _selection = std::monostate{};
      return;
    }

    auto& node = *entry;

    ImGui::SeparatorText(sbx::assets::shader_node_display_name(node.type));

    const auto is_constant = node.type == sbx::assets::shader_node_type::constant_float || node.type == sbx::assets::shader_node_type::constant_vector2 || node.type == sbx::assets::shader_node_type::constant_vector3 || node.type == sbx::assets::shader_node_type::constant_vector4 || node.type == sbx::assets::shader_node_type::constant_color;
    const auto is_texture = node.type == sbx::assets::shader_node_type::texture_sample;

    if (is_constant || is_texture) {
      if (draw_text_field(is_texture ? "Texture Name" : "Parameter Name", node.name)) {
        _apply_live(false); // display-only, not part of the generated Slang
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
          // Exposed constants are read from the material buffer at runtime; non-exposed ones are baked into the Slang and need a recompile.
          _apply_live(!node.exposed);
        }

        break;
      }
      case sbx::assets::shader_node_type::constant_vector2: {
        auto value = std::holds_alternative<sbx::math::vector2>(node.value) ? std::get<sbx::math::vector2>(node.value) : sbx::math::vector2{};
        auto components = std::array<std::float_t, 2u>{value.x(), value.y()};

        if (draw_vector2_control("Value", components, 0.0f, 0.01f).changed) {
          node.value = sbx::math::vector2{components[0], components[1]};
          _apply_live(!node.exposed);
        }

        break;
      }
      case sbx::assets::shader_node_type::constant_vector3: {
        auto value = std::holds_alternative<sbx::math::vector3>(node.value) ? std::get<sbx::math::vector3>(node.value) : sbx::math::vector3{};
        auto components = std::array<std::float_t, 3u>{value.x(), value.y(), value.z()};

        if (draw_vector3_control("Value", components, 0.0f, 0.01f).changed) {
          node.value = sbx::math::vector3{components[0], components[1], components[2]};
          _apply_live(!node.exposed);
        }

        break;
      }
      case sbx::assets::shader_node_type::constant_vector4: {
        // No four-component vector widget exists, so a plain row of DragFloats.
        auto value = std::holds_alternative<sbx::math::vector4>(node.value) ? std::get<sbx::math::vector4>(node.value) : sbx::math::vector4{};
        auto components = std::array<std::float_t, 4u>{value.x(), value.y(), value.z(), value.w()};

        auto changed = false;
        changed |= ImGui::DragFloat("X", &components[0], 0.01f);
        changed |= ImGui::DragFloat("Y", &components[1], 0.01f);
        changed |= ImGui::DragFloat("Z", &components[2], 0.01f);
        changed |= ImGui::DragFloat("W", &components[3], 0.01f);

        if (changed) {
          node.value = sbx::math::vector4{components[0], components[1], components[2], components[3]};
          _apply_live(!node.exposed);
        }

        break;
      }
      case sbx::assets::shader_node_type::constant_color: {
        auto value = std::holds_alternative<sbx::math::color>(node.value) ? std::get<sbx::math::color>(node.value) : sbx::math::color{1.0f, 1.0f, 1.0f, 1.0f};

        if (draw_color_field("Value", value)) {
          node.value = value;
          _apply_live(!node.exposed);
        }

        break;
      }
      case sbx::assets::shader_node_type::texture_sample: {
        // The graph's own default texture, the only one its preview can sample.
        auto value = std::holds_alternative<sbx::assets::texture2d_handle>(node.value) ? std::get<sbx::assets::texture2d_handle>(node.value) : sbx::assets::texture2d_handle{};

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Default Texture");
        ImGui::SameLine(150.0f);

        auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

        if (draw_texture_picker(state, "##shader_graph_default_texture_picker", value, assets_module, sbx::graphics::format::r8g8b8a8_srgb)) {
          node.value = value;
          _apply_live(false); // texture_sample always reads generic_textures at runtime
        }

        // ponytail: only the preview uses this default; new materials with this graph start with an empty slot. Seed them from here if that's wanted.
        ImGui::TextDisabled("Used by this graph's own preview -- a material using this graph still sets its own texture in the Material Inspector.");
        break;
      }
      case sbx::assets::shader_node_type::swizzle: {
        // Four channel dropdowns like Unity's Swizzle node; a text field re-padded the pattern to 4 characters on every keystroke.
        auto pattern = sbx::assets::shader_node_swizzle_pattern(node);

        static constexpr auto channel_names = std::array<const char*, 4u>{"R", "G", "B", "A"};
        static constexpr auto row_labels = std::array<const char*, 4u>{"Output R", "Output G", "Output B", "Output A"};

        auto edited = false;

        for (auto slot = 0u; slot < 4u; ++slot) {
          auto index = pattern[slot] == 'r' ? 0 : pattern[slot] == 'g' ? 1 : pattern[slot] == 'b' ? 2 : 3;

          ImGui::PushID(static_cast<int>(slot));

          if (ImGui::Combo(row_labels[slot], &index, channel_names.data(), static_cast<int>(channel_names.size()))) {
            pattern[slot] = "rgba"[index];
            edited = true;
          }

          ImGui::PopID();
        }

        if (edited) {
          node.value = pattern;
          _apply_live();
        }

        ImGui::TextDisabled("Each output component independently picks which input component feeds it; only as many as the input actually has are used, e.g. swap Output R and Output B to swap red and blue.");
        break;
      }
      default:
        ImGui::TextDisabled("This node has no editable properties.");
        break;
    }

    ImGui::Separator();

    if (ImGui::Button(ICON_MDI_TRASH_CAN " Delete Node")) {
      const auto deleted_id = node.id;

      std::erase_if(_edit.nodes, [deleted_id](const auto& n) { return n.id == deleted_id; });
      std::erase_if(_edit.edges, [deleted_id](const auto& e) { return e.from_node == deleted_id || e.to_node == deleted_id; });

      _seeded_positions.erase(deleted_id);
      _node_previews.forget(deleted_id);
      _selection = std::monostate{};
      _apply_live();
      return;
    }
  } else if (std::holds_alternative<std::size_t>(_selection)) {
    const auto index = std::get<std::size_t>(_selection);

    if (index >= _edit.edges.size()) {
      _selection = std::monostate{};
      return;
    }

    const auto& edge = _edit.edges[index];

    const auto node_label = [this](std::uint32_t id) {
      const auto entry = std::ranges::find(_edit.nodes, id, &sbx::assets::shader_graph_node::id);
      return entry != _edit.nodes.end() ? std::string{sbx::assets::shader_node_display_name(entry->type)} : std::string{"(unknown)"};
    };

    ImGui::SeparatorText("Edge");
    ImGui::Text("From: %s", node_label(edge.from_node).c_str());

    const auto to_it = std::ranges::find(_edit.nodes, edge.to_node, &sbx::assets::shader_graph_node::id);
    const auto to_input_label = (to_it != _edit.nodes.end()) ? std::string{sbx::assets::shader_node_input_label(to_it->type, edge.to_pin)} : std::string{"?"};

    ImGui::Text("To: %s (%s)", node_label(edge.to_node).c_str(), to_input_label.c_str());

    ImGui::Separator();

    if (ImGui::Button(ICON_MDI_TRASH_CAN " Delete Edge")) {
      _edit.edges.erase(_edit.edges.begin() + static_cast<std::ptrdiff_t>(index));
      _selection = std::monostate{};
      _apply_live();
      return;
    }
  } else {
    ImGui::TextDisabled("Select a node or edge.");
    ImGui::Spacing();
    ImGui::TextDisabled("Right-click the canvas to add a node.");
  }
}

} // namespace editor
