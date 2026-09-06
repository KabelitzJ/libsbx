// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/animation_graph_panel.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <string>
#include <unordered_set>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>
#include <libsbx/render/ui/widgets/animation_parameter_widgets.hpp>
#include <libsbx/render/ui/widgets/asset_picker.hpp>

namespace editor {

// NodeId/PinId/LinkId are distinct C++ types, but imgui-node-editor's own hit-testing collapses
// them back to a bare pointer value (EditorContext::UpdateState's checkInteractionsInArea does
// snprintf("%p", id.AsPointer()) and hands that string straight to an ImGui invisible button, with
// no per-kind ID scoping) -- so a node and one of its own pins sharing a raw numeric value *does*
// collide, as a real ImGui ID conflict ("2 visible items with conflicting id"). Every id below
// therefore lives in its own disjoint numeric band, wide enough that no realistic graph (or the
// Any State pseudo-node/its pin, each given a whole band of their own) ever crosses into the next.
constexpr auto node_band = std::uintptr_t{0};
constexpr auto input_pin_band = std::uintptr_t{1'000'000};
constexpr auto output_pin_band = std::uintptr_t{2'000'000};
constexpr auto any_state_band = std::uintptr_t{3'000'000};
constexpr auto link_band = std::uintptr_t{4'000'000};

auto any_state_node_id() -> ax::NodeEditor::NodeId {
  return ax::NodeEditor::NodeId{any_state_band + 1u};
}

auto any_state_output_pin() -> ax::NodeEditor::PinId {
  return ax::NodeEditor::PinId{any_state_band + 2u};
}

// +1 on every band keeps every real id >= 1 (0 is imgui-node-editor's "invalid" sentinel).
auto node_id_for_state(std::uint32_t state_id) -> ax::NodeEditor::NodeId {
  return ax::NodeEditor::NodeId{node_band + static_cast<std::uintptr_t>(state_id) + 1u};
}

// nullopt = the Any State pseudo-node (or an invalid/stale id).
auto state_id_from_node(ax::NodeEditor::NodeId id) -> std::optional<std::uint32_t> {
  const auto raw = id.Get();

  if (raw <= node_band || raw >= input_pin_band) {
    return std::nullopt;
  }

  return static_cast<std::uint32_t>(raw - node_band - 1u);
}

auto input_pin_for_state(std::uint32_t state_id) -> ax::NodeEditor::PinId {
  return ax::NodeEditor::PinId{input_pin_band + static_cast<std::uintptr_t>(state_id) + 1u};
}

auto output_pin_for_state(std::uint32_t state_id) -> ax::NodeEditor::PinId {
  return ax::NodeEditor::PinId{output_pin_band + static_cast<std::uintptr_t>(state_id) + 1u};
}

struct resolved_pin {
  bool is_any_state{false};
  bool is_output{false};
  std::uint32_t state_id{0u};
}; // struct resolved_pin

auto resolve_pin(ax::NodeEditor::PinId pin) -> resolved_pin {
  const auto raw = pin.Get();

  if (raw >= any_state_band) {
    return resolved_pin{.is_any_state = true, .is_output = true, .state_id = 0u};
  }

  if (raw > output_pin_band) {
    return resolved_pin{.is_any_state = false, .is_output = true, .state_id = static_cast<std::uint32_t>(raw - output_pin_band - 1u)};
  }

  if (raw > input_pin_band) {
    return resolved_pin{.is_any_state = false, .is_output = false, .state_id = static_cast<std::uint32_t>(raw - input_pin_band - 1u)};
  }

  return resolved_pin{.is_any_state = true, .is_output = true, .state_id = 0u}; // invalid/stale id -- treat as harmless no-op
}

// transition index -> LinkId and back; +1 keeps every link id >= 1 (0 is "invalid").
auto link_id_for_transition(std::size_t index) -> ax::NodeEditor::LinkId {
  return ax::NodeEditor::LinkId{link_band + index + 1u};
}

auto transition_index_from_link(ax::NodeEditor::LinkId id) -> std::optional<std::size_t> {
  const auto raw = id.Get();

  if (raw <= link_band) {
    return std::nullopt;
  }

  return raw - link_band - 1u;
}

// Same idea as inspector_panel.cpp's relative_asset_path, kept local here rather than shared --
// this file's only use of it is the preview-mesh picker below.
auto relative_mesh_path(sbx::assets::assets_module& assets_module, const sbx::math::uuid& id) -> std::filesystem::path {
  const auto path = assets_module.path_of(id);

  if (path.empty()) {
    return {};
  }

  auto& project = sbx::core::engine::project();
  const auto relative = std::filesystem::relative(path, project.assets_directory());

  if (relative.empty() || relative.begin()->string() == "..") {
    return {};
  }

  return relative;
}

auto sync_text_field(const char* label, std::string& value) -> bool {
  auto buffer = std::array<char, 128u>{};
  std::strncpy(buffer.data(), value.c_str(), buffer.size() - 1u);
  buffer[buffer.size() - 1u] = '\0';

  if (ImGui::InputText(label, buffer.data(), buffer.size())) {
    value = buffer.data();
    return true;
  }

  return false;
}

animation_graph_panel::animation_graph_panel() {
  auto config = ax::NodeEditor::Config{};
  config.SettingsFile = nullptr; // node positions round-trip through animation_state::editor_position instead

  _context = ax::NodeEditor::CreateEditor(&config);
}

animation_graph_panel::~animation_graph_panel() {
  ax::NodeEditor::DestroyEditor(_context);
}

auto animation_graph_panel::_open(sbx::assets::animation_graph_handle graph, std::filesystem::path path, sbx::assets::mesh_handle preview_mesh) -> void {
  _graph = std::move(graph);
  _path = std::move(path);
  _is_open = true;
  _selection = std::monostate{};
  _seeded_positions.clear();
  _any_state_seeded = false;
  _preview_mesh = std::move(preview_mesh);

  if (_graph.is_valid()) {
    _edit.name = _graph->name();
    _edit.parameters = _graph->parameters();
    _edit.states = _graph->states();
    _edit.transitions = _graph->transitions();
    _edit.entry_state_id = _graph->entry_state_id();
  } else {
    _edit = sbx::assets::animation_graph::create_info{};
  }
}

auto animation_graph_panel::_apply_live() -> void {
  if (!_graph.is_valid()) {
    return;
  }

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  assets_module.update_animation_graph(_graph, _edit);
}

auto animation_graph_panel::_state_name(std::uint32_t state_id) const -> std::string {
  const auto it = std::ranges::find(_edit.states, state_id, &sbx::assets::animation_state::id);
  return (it != _edit.states.end()) ? it->name : std::string{"(unknown)"};
}

auto animation_graph_panel::draw(editor_state& state) -> void {
  if (state.open_animation_graph_request.has_value()) {
    const auto request = *state.open_animation_graph_request;
    state.open_animation_graph_request.reset();

    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

    auto preview_mesh = sbx::assets::mesh_handle{};
    if (request.preview_mesh_id != sbx::math::uuid::nil()) {
      preview_mesh = assets_module.load_mesh(request.preview_mesh_id);
    }

    _open(assets_module.load_animation_graph(request.id), request.path, preview_mesh);
  }

  if (!_is_open) {
    return;
  }

  const auto title = fmt::format("{} Animation Graph \xe2\x80\x94 {}###animation_graph_panel", ICON_MDI_STATE_MACHINE, _path.stem().string());

  ImGui::SetNextWindowSize(ImVec2{900.0f, 600.0f}, ImGuiCond_FirstUseEver);

  if (!ImGui::Begin(title.c_str(), &_is_open)) {
    ImGui::End();
    return;
  }

  if (!_graph.is_valid()) {
    ImGui::TextDisabled("Could not load this animation graph.");
    ImGui::End();
    return;
  }

  _draw_toolbar();

  ImGui::Separator();

  _draw_parameters();

  ImGui::Separator();

  const auto region = ImGui::GetContentRegionAvail();
  constexpr auto inspector_width = 320.0f;

  ImGui::BeginChild("##animation_graph_canvas_region", ImVec2{region.x - inspector_width, 0.0f}, ImGuiChildFlags_Borders);
  _draw_canvas();
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("##animation_graph_selection_region", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_Borders);
  _draw_selection_inspector();
  ImGui::EndChild();

  ImGui::End();
}

auto animation_graph_panel::_draw_toolbar() -> void {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
    assets_module.save_animation_graph(_graph, _path);
  }

  ImGui::SameLine();
  ImGui::TextUnformatted("Preview Mesh:");
  ImGui::SameLine();

  // Editor-only, not part of the asset -- lets Clip Name (below) list real clip names instead of
  // being free text. Seeded from whatever mesh was in scope when this editor was opened.
  const auto current = _preview_mesh.is_valid() ? sbx::render::widgets::asset_picker_item{_preview_mesh->id(), relative_mesh_path(assets_module, _preview_mesh->id())} : sbx::render::widgets::asset_picker_item{};

  const auto options = sbx::render::widgets::asset_picker_options{
    .kind = sbx::render::widgets::asset_picker_kind::mesh,
    .extensions = {".gltf", ".glb"},
    .allow_none = true,
  };

  const auto result = sbx::render::widgets::draw_asset_picker("##animation_graph_preview_mesh", current, {}, options);

  if (result.cleared) {
    _preview_mesh = sbx::assets::mesh_handle{};
  } else if (result.changed) {
    _preview_mesh = assets_module.load_mesh(result.picked.path);
  }
}

auto animation_graph_panel::_draw_parameters() -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_TUNE " Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  static constexpr auto type_names = std::array<const char*, 4u>{"Float", "Bool", "Int", "Trigger"};

  auto removed_index = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < _edit.parameters.size(); ++index) {
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& parameter = _edit.parameters[index];

    ImGui::SetNextItemWidth(140.0f);
    if (sync_text_field("##name", parameter.name)) {
      _apply_live();
    }

    ImGui::SameLine();

    auto type_index = static_cast<std::int32_t>(parameter.default_value.index());
    ImGui::SetNextItemWidth(90.0f);

    if (ImGui::Combo("##type", &type_index, type_names.data(), static_cast<std::int32_t>(type_names.size()))) {
      switch (type_index) {
        case 0: parameter.default_value = std::float_t{0.0f}; break;
        case 1: parameter.default_value = false; break;
        case 2: parameter.default_value = std::int32_t{0}; break;
        default: parameter.default_value = sbx::assets::animation_trigger{}; break;
      }

      _apply_live();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);

    if (sbx::render::widgets::draw_animation_parameter_value("##value", parameter.default_value)) {
      _apply_live();
    }

    ImGui::SameLine();

    if (ImGui::Button(ICON_MDI_TRASH_CAN)) {
      removed_index = index;
    }

    ImGui::PopID();
  }

  if (removed_index.has_value()) {
    const auto removed_name = _edit.parameters[*removed_index].name;
    _edit.parameters.erase(_edit.parameters.begin() + static_cast<std::ptrdiff_t>(*removed_index));

    // A condition referencing a deleted parameter would otherwise just never match
    // (evaluate_animation_condition's "unknown parameter" fallback) -- drop those too rather than
    // leave dead references around.
    for (auto& transition : _edit.transitions) {
      std::erase_if(transition.conditions, [&removed_name](const auto& condition) { return condition.parameter_name == removed_name; });
    }

    _apply_live();
  }

  if (ImGui::Button(ICON_MDI_PLUS " Add Parameter")) {
    auto name = std::string{"Param"};
    auto suffix = 1;

    while (std::ranges::any_of(_edit.parameters, [&name](const auto& parameter) { return parameter.name == name; })) {
      name = fmt::format("Param {}", suffix++);
    }

    _edit.parameters.push_back(sbx::assets::animation_parameter{.name = name, .default_value = 0.0f});
    _apply_live();
  }
}

constexpr auto input_pin_color = IM_COL32(94, 174, 255, 255);
constexpr auto output_pin_color = IM_COL32(255, 176, 79, 255);

// Unreal-Blueprint-style pin: a small circle, filled once at least one transition is attached to
// it and hollow (outline only) otherwise -- replaces the plain arrow glyph the pins used to be.
auto draw_pin_icon(bool connected, ImU32 color) -> void {
  constexpr auto diameter = 11.0f;

  // Reserved box is diameter wide but text-line-height tall, and the circle is centered within
  // that height -- lines it up with the label text's SameLine row instead of its own (shorter)
  // diameter, which used to sit a few pixels above the label's vertical center.
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

auto animation_graph_panel::_draw_canvas() -> void {
  ax::NodeEditor::SetCurrentEditor(_context);
  ax::NodeEditor::Begin("##animation_graph_node_canvas", ImVec2{0.0f, 0.0f});

  if (!_any_state_seeded) {
    ax::NodeEditor::SetNodePosition(any_state_node_id(), ImVec2{-260.0f, 0.0f});
    _any_state_seeded = true;
  }

  // Which pins currently have at least one transition attached -- drives the filled-vs-hollow
  // look of draw_pin_icon below.
  auto connected_inputs = std::unordered_set<std::uint32_t>{};
  auto connected_outputs = std::unordered_set<std::uint32_t>{};
  auto any_state_connected = false;

  for (const auto& transition : _edit.transitions) {
    connected_inputs.insert(transition.to_state);

    if (transition.from_state.has_value()) {
      connected_outputs.insert(*transition.from_state);
    } else {
      any_state_connected = true;
    }
  }

  ax::NodeEditor::BeginNode(any_state_node_id());
  ImGui::TextUnformatted(ICON_MDI_FLAG " Any State");
  ImGui::SameLine();
  ax::NodeEditor::BeginPin(any_state_output_pin(), ax::NodeEditor::PinKind::Output);
  draw_pin_icon(any_state_connected, output_pin_color);
  ax::NodeEditor::EndPin();
  ax::NodeEditor::EndNode();

  for (auto& state : _edit.states) {
    const auto node_id = node_id_for_state(state.id);

    if (!_seeded_positions.contains(state.id)) {
      ax::NodeEditor::SetNodePosition(node_id, ImVec2{state.editor_position.x(), state.editor_position.y()});
      _seeded_positions.insert(state.id);
    }

    ax::NodeEditor::BeginNode(node_id);

    ax::NodeEditor::BeginPin(input_pin_for_state(state.id), ax::NodeEditor::PinKind::Input);
    draw_pin_icon(connected_inputs.contains(state.id), input_pin_color);
    ax::NodeEditor::EndPin();

    ImGui::SameLine();

    if (state.id == _edit.entry_state_id) {
      ImGui::TextUnformatted(ICON_MDI_STAR);
      ImGui::SameLine();
    }

    ImGui::TextUnformatted(state.name.empty() ? "(unnamed)" : state.name.c_str());

    ImGui::SameLine();

    ax::NodeEditor::BeginPin(output_pin_for_state(state.id), ax::NodeEditor::PinKind::Output);
    draw_pin_icon(connected_outputs.contains(state.id), output_pin_color);
    ax::NodeEditor::EndPin();

    ax::NodeEditor::EndNode();

    const auto position = ax::NodeEditor::GetNodePosition(node_id);

    if (position.x != state.editor_position.x() || position.y != state.editor_position.y()) {
      state.editor_position = sbx::math::vector2{position.x, position.y};
      _apply_live();
    }
  }

  for (auto index = std::size_t{0u}; index < _edit.transitions.size(); ++index) {
    const auto& transition = _edit.transitions[index];
    const auto source_pin = transition.from_state.has_value() ? output_pin_for_state(*transition.from_state) : any_state_output_pin();

    ax::NodeEditor::Link(link_id_for_transition(index), source_pin, input_pin_for_state(transition.to_state));
  }

  if (ax::NodeEditor::BeginCreate()) {
    auto start_pin = ax::NodeEditor::PinId{};
    auto end_pin = ax::NodeEditor::PinId{};

    if (ax::NodeEditor::QueryNewLink(&start_pin, &end_pin)) {
      const auto start = resolve_pin(start_pin);
      const auto end = resolve_pin(end_pin);

      // A drag can go in either direction -- normalize by pin kind rather than start/end order.
      const auto* source = start.is_output ? &start : (end.is_output ? &end : nullptr);
      const auto* target = start.is_output ? &end : (end.is_output ? &start : nullptr);

      if (source == nullptr || target == nullptr || target == source || target->is_output || target->is_any_state) {
        ax::NodeEditor::RejectNewItem(ImVec4{1.0f, 0.3f, 0.3f, 1.0f});
      } else if (ax::NodeEditor::AcceptNewItem()) {
        auto transition = sbx::assets::animation_transition{};
        transition.from_state = source->is_any_state ? std::nullopt : std::optional<std::uint32_t>{source->state_id};
        transition.to_state = target->state_id;

        _edit.transitions.push_back(transition);
        _selection = _edit.transitions.size() - 1u;
        _apply_live();
      }
    }
  }

  ax::NodeEditor::EndCreate();

  if (ax::NodeEditor::BeginDelete()) {
    auto link_id = ax::NodeEditor::LinkId{};

    while (ax::NodeEditor::QueryDeletedLink(&link_id)) {
      const auto transition_index = transition_index_from_link(link_id);

      if (transition_index.has_value() && ax::NodeEditor::AcceptDeletedItem()) {
        if (*transition_index < _edit.transitions.size()) {
          _edit.transitions.erase(_edit.transitions.begin() + static_cast<std::ptrdiff_t>(*transition_index));
        }

        _selection = std::monostate{};
        _apply_live();
      }
    }

    auto node_id = ax::NodeEditor::NodeId{};

    while (ax::NodeEditor::QueryDeletedNode(&node_id)) {
      const auto deleted_state_id = state_id_from_node(node_id);

      // The Any State pseudo-node isn't part of the asset -- leave it undeleted by simply never
      // accepting its deletion query.
      if (deleted_state_id.has_value() && ax::NodeEditor::AcceptDeletedItem()) {
        std::erase_if(_edit.states, [&deleted_state_id](const auto& s) { return s.id == *deleted_state_id; });
        std::erase_if(_edit.transitions, [&deleted_state_id](const auto& t) { return t.to_state == *deleted_state_id || t.from_state == *deleted_state_id; });

        if (_edit.entry_state_id == *deleted_state_id && !_edit.states.empty()) {
          _edit.entry_state_id = _edit.states.front().id;
        }

        _seeded_positions.erase(*deleted_state_id);
        _selection = std::monostate{};
        _apply_live();
      }
    }
  }

  ax::NodeEditor::EndDelete();

  const auto spawn_position = ImGui::GetMousePos(); // captured before Suspend, screen-space -- same convention imgui-node-editor's own blueprints example uses for a new node's initial position

  auto context_node_id = ax::NodeEditor::NodeId{};

  ax::NodeEditor::Suspend();

  if (ax::NodeEditor::ShowNodeContextMenu(&context_node_id)) {
    ImGui::OpenPopup("##animation_graph_node_context");
  } else if (ax::NodeEditor::ShowBackgroundContextMenu()) {
    ImGui::OpenPopup("##animation_graph_background_context");
  }

  if (ImGui::BeginPopup("##animation_graph_node_context")) {
    const auto context_state_id = state_id_from_node(context_node_id);

    if (context_state_id.has_value()) {
      if (ImGui::MenuItem(ICON_MDI_STAR " Set as Entry State", nullptr, false, _edit.entry_state_id != *context_state_id)) {
        _edit.entry_state_id = *context_state_id;
        _apply_live();
      }

      if (ImGui::MenuItem(ICON_MDI_TRASH_CAN " Delete State")) {
        ax::NodeEditor::DeleteNode(context_node_id); // reconciled by BeginDelete/QueryDeletedNode next frame
      }
    } else {
      ImGui::TextDisabled("Any State");
    }

    ImGui::EndPopup();
  }

  if (ImGui::BeginPopup("##animation_graph_background_context")) {
    if (ImGui::MenuItem(ICON_MDI_PLUS " Add State")) {
      auto next_id = std::uint32_t{0u};

      for (const auto& existing : _edit.states) {
        next_id = std::max(next_id, existing.id + 1u);
      }

      auto new_state = sbx::assets::animation_state{};
      new_state.id = next_id;
      new_state.name = fmt::format("State {}", next_id);
      new_state.editor_position = sbx::math::vector2{spawn_position.x, spawn_position.y};

      _edit.states.push_back(new_state);

      if (_edit.states.size() == 1u) {
        _edit.entry_state_id = new_state.id;
      }

      _selection = new_state.id;
      _apply_live();
    }

    ImGui::EndPopup();
  }

  ax::NodeEditor::Resume();

  auto selected_node = ax::NodeEditor::NodeId{};
  auto selected_link = ax::NodeEditor::LinkId{};

  if (ax::NodeEditor::GetSelectedNodes(&selected_node, 1) > 0) {
    if (const auto selected_state_id = state_id_from_node(selected_node)) {
      _selection = *selected_state_id;
    }
  } else if (ax::NodeEditor::GetSelectedLinks(&selected_link, 1) > 0) {
    if (const auto selected_transition_index = transition_index_from_link(selected_link)) {
      _selection = *selected_transition_index;
    }
  } else {
    _selection = std::monostate{};
  }

  ax::NodeEditor::End();
}

auto animation_graph_panel::_draw_selection_inspector() -> void {
  if (std::holds_alternative<std::uint32_t>(_selection)) {
    const auto state_id = std::get<std::uint32_t>(_selection);
    const auto it = std::ranges::find(_edit.states, state_id, &sbx::assets::animation_state::id);

    if (it == _edit.states.end()) {
      _selection = std::monostate{};
      return;
    }

    auto& selected_state = *it;

    ImGui::SeparatorText("State");

    if (sync_text_field("Name", selected_state.name)) {
      _apply_live();
    }

    const auto* preview_clips = (_preview_mesh.is_valid() && !_preview_mesh->animation_clips().empty()) ? &_preview_mesh->animation_clips() : nullptr;

    if (preview_clips != nullptr) {
      // Picked from the preview mesh's real clip list rather than free text -- clip_name still
      // resolves by name at runtime (see assets::animation_state's doc comment), this just removes
      // the typo risk of hand-typing it.
      if (ImGui::BeginCombo("Clip Name", selected_state.clip_name.empty() ? "(none)" : selected_state.clip_name.c_str())) {
        for (const auto& clip : *preview_clips) {
          if (!clip.is_valid()) {
            continue;
          }

          const auto is_selected = clip->name() == selected_state.clip_name;

          if (ImGui::Selectable(clip->name().c_str(), is_selected)) {
            selected_state.clip_name = clip->name();
            _apply_live();
          }
        }

        ImGui::EndCombo();
      }
    } else {
      if (sync_text_field("Clip Name", selected_state.clip_name)) {
        _apply_live();
      }

      ImGui::TextDisabled("Set a Preview Mesh above to pick from a list instead.");
    }

    if (!selected_state.clip_name.empty() && preview_clips != nullptr) {
      const auto resolves = std::ranges::any_of(*preview_clips, [&selected_state](const auto& clip) { return clip.is_valid() && clip->name() == selected_state.clip_name; });

      if (!resolves) {
        ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f}, ICON_MDI_ALERT " No clip named '%s' on the preview mesh -- this state will hold its bind pose at runtime.", selected_state.clip_name.c_str());
      }
    }

    if (ImGui::DragFloat("Speed", &selected_state.speed, 0.05f, 0.0f, 10.0f)) {
      _apply_live();
    }

    if (ImGui::Checkbox("Loop", &selected_state.loop)) {
      _apply_live();
    }

    ImGui::BeginDisabled(_edit.entry_state_id == selected_state.id);

    if (ImGui::Button(ICON_MDI_STAR " Set as Entry State")) {
      _edit.entry_state_id = selected_state.id;
      _apply_live();
    }

    ImGui::EndDisabled();
  } else if (std::holds_alternative<std::size_t>(_selection)) {
    const auto index = std::get<std::size_t>(_selection);

    if (index >= _edit.transitions.size()) {
      _selection = std::monostate{};
      return;
    }

    auto& transition = _edit.transitions[index];

    ImGui::SeparatorText("Transition");

    ImGui::Text("From: %s", transition.from_state.has_value() ? _state_name(*transition.from_state).c_str() : "Any State");
    ImGui::Text("To: %s", _state_name(transition.to_state).c_str());

    if (ImGui::DragFloat("Duration", &transition.duration, 0.01f, 0.0f, 10.0f)) {
      _apply_live();
    }

    if (ImGui::Checkbox("Has Exit Time", &transition.has_exit_time)) {
      _apply_live();
    }

    ImGui::BeginDisabled(!transition.has_exit_time);

    if (ImGui::DragFloat("Exit Time", &transition.exit_time, 0.01f, 0.0f, 1.0f)) {
      _apply_live();
    }

    ImGui::EndDisabled();

    ImGui::SeparatorText("Conditions");

    auto removed_condition = std::optional<std::size_t>{};

    for (auto condition_index = std::size_t{0u}; condition_index < transition.conditions.size(); ++condition_index) {
      ImGui::PushID(static_cast<std::int32_t>(condition_index));

      auto& condition = transition.conditions[condition_index];

      auto current_parameter_index = std::int32_t{-1};

      for (auto p = std::size_t{0u}; p < _edit.parameters.size(); ++p) {
        if (_edit.parameters[p].name == condition.parameter_name) {
          current_parameter_index = static_cast<std::int32_t>(p);
          break;
        }
      }

      const auto* preview = (current_parameter_index >= 0) ? _edit.parameters[static_cast<std::size_t>(current_parameter_index)].name.c_str() : "(unknown)";

      if (ImGui::BeginCombo("Parameter", preview)) {
        for (auto p = std::size_t{0u}; p < _edit.parameters.size(); ++p) {
          const auto is_selected = static_cast<std::int32_t>(p) == current_parameter_index;

          if (ImGui::Selectable(_edit.parameters[p].name.c_str(), is_selected)) {
            condition.parameter_name = _edit.parameters[p].name;
            condition.expected = sbx::render::widgets::default_for_same_alternative(_edit.parameters[p].default_value);
            _apply_live();
          }
        }

        ImGui::EndCombo();
      }

      const auto is_trigger = current_parameter_index >= 0 && std::holds_alternative<sbx::assets::animation_trigger>(_edit.parameters[static_cast<std::size_t>(current_parameter_index)].default_value);
      const auto is_bool = current_parameter_index >= 0 && std::holds_alternative<bool>(_edit.parameters[static_cast<std::size_t>(current_parameter_index)].default_value);

      if (!is_trigger) {
        if (is_bool) {
          // Ordering/Greater-Less don't mean anything for a bool -- only equality does.
          static constexpr auto bool_comparator_names = std::array<const char*, 2u>{"Equals", "Not Equals"};
          auto comparator_index = (condition.comparator == sbx::assets::animation_condition_comparator::not_equals) ? 1 : 0;

          if (ImGui::Combo("Comparator", &comparator_index, bool_comparator_names.data(), static_cast<std::int32_t>(bool_comparator_names.size()))) {
            condition.comparator = (comparator_index == 1) ? sbx::assets::animation_condition_comparator::not_equals : sbx::assets::animation_condition_comparator::equals;
            _apply_live();
          }
        } else {
          static constexpr auto comparator_names = std::array<const char*, 6u>{"Equals", "Not Equals", "Greater", "Greater or Equal", "Less", "Less or Equal"};
          auto comparator_index = static_cast<std::int32_t>(condition.comparator);

          if (ImGui::Combo("Comparator", &comparator_index, comparator_names.data(), static_cast<std::int32_t>(comparator_names.size()))) {
            condition.comparator = static_cast<sbx::assets::animation_condition_comparator>(comparator_index);
            _apply_live();
          }
        }

        if (sbx::render::widgets::draw_animation_parameter_value("Value", condition.expected)) {
          _apply_live();
        }
      } else {
        ImGui::TextDisabled("Fires when this trigger is set.");
      }

      if (ImGui::Button(ICON_MDI_TRASH_CAN " Remove Condition")) {
        removed_condition = condition_index;
      }

      ImGui::Separator();
      ImGui::PopID();
    }

    if (removed_condition.has_value()) {
      transition.conditions.erase(transition.conditions.begin() + static_cast<std::ptrdiff_t>(*removed_condition));
      _apply_live();
    }

    ImGui::BeginDisabled(_edit.parameters.empty());

    if (ImGui::Button(ICON_MDI_PLUS " Add Condition")) {
      transition.conditions.push_back(sbx::assets::animation_condition{
        .parameter_name = _edit.parameters.front().name,
        .expected = sbx::render::widgets::default_for_same_alternative(_edit.parameters.front().default_value)
      });

      _apply_live();
    }

    ImGui::EndDisabled();

    ImGui::Separator();

    if (ImGui::Button(ICON_MDI_TRASH_CAN " Delete Transition")) {
      _edit.transitions.erase(_edit.transitions.begin() + static_cast<std::ptrdiff_t>(index));
      _selection = std::monostate{};
      _apply_live();
    }
  } else {
    ImGui::TextDisabled("Select a state or transition.");
  }
}

} // namespace editor
