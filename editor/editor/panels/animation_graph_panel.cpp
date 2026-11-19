// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/animation_graph_panel.hpp>

#include <libsbx/utility/profiler.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/animation/pose_evaluation.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>
#include <libsbx/render/ui/widgets/animation_parameter_widgets.hpp>
#include <libsbx/render/ui/widgets/asset_picker.hpp>

#include <editor/widgets/asset_path.hpp>
#include <editor/widgets/text_field.hpp>

namespace editor {

// imgui-node-editor's hit-testing reduces node, pin and link ids to bare pointer values, so a node and its pin with the same number collide as ImGui ids. Each kind gets its own disjoint band.
constexpr auto node_band = std::uintptr_t{0};
constexpr auto input_pin_band = std::uintptr_t{1'000'000};
constexpr auto output_pin_band = std::uintptr_t{2'000'000};
constexpr auto any_state_band = std::uintptr_t{3'000'000};
constexpr auto link_band = std::uintptr_t{4'000'000};

static auto any_state_node_id() -> ax::NodeEditor::NodeId {
  return ax::NodeEditor::NodeId{any_state_band + 1u};
}

static auto any_state_output_pin() -> ax::NodeEditor::PinId {
  return ax::NodeEditor::PinId{any_state_band + 2u};
}

// +1 keeps every id >= 1; 0 is imgui-node-editor's invalid sentinel.
static auto node_id_for_state(std::uint32_t state_id) -> ax::NodeEditor::NodeId {
  return ax::NodeEditor::NodeId{node_band + static_cast<std::uintptr_t>(state_id) + 1u};
}

// nullopt for the Any State pseudo-node or a stale id.
static auto state_id_from_node(ax::NodeEditor::NodeId id) -> std::optional<std::uint32_t> {
  const auto raw = id.Get();

  if (raw <= node_band || raw >= input_pin_band) {
    return std::nullopt;
  }

  return static_cast<std::uint32_t>(raw - node_band - 1u);
}

static auto input_pin_for_state(std::uint32_t state_id) -> ax::NodeEditor::PinId {
  return ax::NodeEditor::PinId{input_pin_band + static_cast<std::uintptr_t>(state_id) + 1u};
}

static auto output_pin_for_state(std::uint32_t state_id) -> ax::NodeEditor::PinId {
  return ax::NodeEditor::PinId{output_pin_band + static_cast<std::uintptr_t>(state_id) + 1u};
}

struct resolved_pin {
  bool is_any_state{false};
  bool is_output{false};
  std::uint32_t state_id{0u};
}; // struct resolved_pin

static auto resolve_pin(ax::NodeEditor::PinId pin) -> resolved_pin {
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

  return resolved_pin{.is_any_state = true, .is_output = true, .state_id = 0u}; // stale id: a harmless no-op
}

// Transition index <-> LinkId; +1 keeps ids >= 1.
static auto link_id_for_transition(std::size_t index) -> ax::NodeEditor::LinkId {
  return ax::NodeEditor::LinkId{link_band + index + 1u};
}

static auto transition_index_from_link(ax::NodeEditor::LinkId id) -> std::optional<std::size_t> {
  const auto raw = id.Get();

  if (raw <= link_band) {
    return std::nullopt;
  }

  return raw - link_band - 1u;
}

animation_graph_panel::animation_graph_panel() {
  auto config = ax::NodeEditor::Config{};
  config.SettingsFile = nullptr; // node positions are stored in animation_state::editor_position

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
    _edit.layers = _graph->layers();
  } else {
    _edit = sbx::assets::animation_graph::create_info{};
  }

  if (_edit.layers.empty()) {
    _edit.layers.emplace_back();
  }

  _layer_index = 0u;
  _select_layer_tab = true;
}

auto animation_graph_panel::_apply_live() -> void {
  if (!_graph.is_valid()) {
    return;
  }

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  assets_module.update_animation_graph(_graph, _edit);
}

auto animation_graph_panel::_draw_float_parameter_combo(const char* label, std::string& name) -> bool {
  auto changed = false;

  if (ImGui::BeginCombo(label, name.empty() ? "(none)" : name.c_str())) {
    if (ImGui::Selectable("(none)", name.empty()) && !name.empty()) {
      name.clear();
      changed = true;
    }

    for (const auto& parameter : _edit.parameters) {
      const auto is_number = std::holds_alternative<std::float_t>(parameter.default_value) || std::holds_alternative<std::int32_t>(parameter.default_value);

      if (!is_number) {
        continue;
      }

      if (ImGui::Selectable(parameter.name.c_str(), parameter.name == name) && parameter.name != name) {
        name = parameter.name;
        changed = true;
      }
    }

    ImGui::EndCombo();
  }

  return changed;
}

auto animation_graph_panel::_draw_blend_preview(const sbx::assets::animation_motion& motion, std::span<const std::float_t> weights) -> void {
  auto values = std::vector<std::pair<std::string, sbx::assets::animation_parameter_value>>{};

  for (const auto& parameter : _edit.parameters) {
    values.emplace_back(parameter.name, parameter.default_value);
  }

  const auto value_of = [&](const std::string& name) {
    const auto entry = std::ranges::find(values, name, &std::pair<std::string, sbx::assets::animation_parameter_value>::first);

    if (entry == values.end()) {
      return 0.0f;
    }

    if (const auto* value = std::get_if<std::float_t>(&entry->second)) {
      return *value;
    }

    if (const auto* value = std::get_if<std::int32_t>(&entry->second)) {
      return static_cast<std::float_t>(*value);
    }

    return 0.0f;
  };

  const auto point = ImVec2{value_of(motion.parameter_x), value_of(motion.parameter_y)};

  // Fit every child and the point, keeping the origin centered.
  auto extent = std::max(std::abs(point.x), std::abs(point.y));

  for (const auto& child : motion.children) {
    extent = std::max({extent, std::abs(child.position.x()), std::abs(child.position.y())});
  }

  extent = std::max(extent * 1.15f, 0.5f);

  const auto size = std::min(ImGui::GetContentRegionAvail().x, 180.0f);
  const auto origin = ImGui::GetCursorScreenPos();
  const auto center = ImVec2{origin.x + size * 0.5f, origin.y + size * 0.5f};

  const auto to_screen = [&](float x, float y) {
    return ImVec2{center.x + x / extent * size * 0.5f, center.y - y / extent * size * 0.5f};
  };

  auto* draw_list = ImGui::GetWindowDrawList();

  draw_list->AddRectFilled(origin, ImVec2{origin.x + size, origin.y + size}, IM_COL32(30, 30, 34, 255));
  draw_list->AddLine(ImVec2{origin.x, center.y}, ImVec2{origin.x + size, center.y}, IM_COL32(70, 70, 80, 255));
  draw_list->AddLine(ImVec2{center.x, origin.y}, ImVec2{center.x, origin.y + size}, IM_COL32(70, 70, 80, 255));

  for (auto index = std::size_t{0u}; index < weights.size(); ++index) {
    const auto& child = motion.children[index];
    const auto position = to_screen(child.position.x(), child.position.y());

    draw_list->AddCircleFilled(position, 3.0f + weights[index] * 7.0f, IM_COL32(90, 160, 255, static_cast<int>(120.0f + weights[index] * 135.0f)));
    draw_list->AddText(ImVec2{position.x + 6.0f, position.y - 6.0f}, IM_COL32(200, 200, 210, 255), fmt::format("{}", index).c_str());
  }

  const auto marker = to_screen(point.x, point.y);

  draw_list->AddLine(ImVec2{marker.x - 5.0f, marker.y}, ImVec2{marker.x + 5.0f, marker.y}, IM_COL32(255, 200, 60, 255), 2.0f);
  draw_list->AddLine(ImVec2{marker.x, marker.y - 5.0f}, ImVec2{marker.x, marker.y + 5.0f}, IM_COL32(255, 200, 60, 255), 2.0f);

  ImGui::Dummy(ImVec2{size, size});
  ImGui::TextDisabled("Cross: the parameters' default values.");
}

auto animation_graph_panel::_draw_motion(sbx::assets::animation_motion& motion) -> bool {
  auto changed = false;

  if (motion.kind == sbx::assets::animation_motion_kind::clip) {
    const auto* preview_clips = (_preview_mesh.is_valid() && !_preview_mesh->animation_clips().empty()) ? &_preview_mesh->animation_clips() : nullptr;

    if (preview_clips != nullptr) {
      // Picked from the preview mesh's clips to avoid typos; clips still resolve by name at runtime.
      if (ImGui::BeginCombo("Clip", motion.clip_name.empty() ? "(none)" : motion.clip_name.c_str())) {
        for (const auto& clip : *preview_clips) {
          if (!clip.is_valid()) {
            continue;
          }

          if (ImGui::Selectable(clip->name().c_str(), clip->name() == motion.clip_name)) {
            motion.clip_name = clip->name();
            changed = true;
          }
        }

        ImGui::EndCombo();
      }

      if (!motion.clip_name.empty()) {
        const auto resolves = std::ranges::any_of(*preview_clips, [&motion](const auto& clip) { return clip.is_valid() && clip->name() == motion.clip_name; });

        if (!resolves) {
          ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f}, ICON_MDI_ALERT " No clip named '%s' on the preview mesh; it holds the bind pose at runtime.", motion.clip_name.c_str());
        }
      }
    } else {
      changed |= draw_text_field("Clip", motion.clip_name);
      ImGui::TextDisabled("Set a Preview Mesh above to pick from a list instead.");
    }

    return changed;
  }

  static constexpr auto blend_labels = std::array<const char*, 4u>{"1D", "2D Freeform Directional", "2D Freeform Cartesian", "Direct"};

  auto blend_type = static_cast<int>(motion.blend_type);

  if (ImGui::Combo("Blend Type", &blend_type, blend_labels.data(), static_cast<int>(blend_labels.size()))) {
    motion.blend_type = static_cast<sbx::assets::animation_blend_type>(blend_type);
    changed = true;
  }

  const auto is_2d = motion.blend_type == sbx::assets::animation_blend_type::freeform_directional_2d || motion.blend_type == sbx::assets::animation_blend_type::freeform_cartesian_2d;

  if (motion.blend_type != sbx::assets::animation_blend_type::direct) {
    changed |= _draw_float_parameter_combo(is_2d ? "Parameter X" : "Parameter", motion.parameter_x);
  }

  if (is_2d) {
    changed |= _draw_float_parameter_combo("Parameter Y", motion.parameter_y);
  }

  // Weights at the parameters' default values, to see what the tree does without running the game.
  auto defaults = std::vector<std::pair<std::string, sbx::assets::animation_parameter_value>>{};

  for (const auto& parameter : _edit.parameters) {
    defaults.emplace_back(parameter.name, parameter.default_value);
  }

  const auto count = std::min(motion.children.size(), sbx::assets::max_blend_children);

  auto weights = std::array<std::float_t, sbx::assets::max_blend_children>{};

  sbx::animation::blend_child_weights(motion, defaults, std::span{weights.data(), count});

  if (is_2d && count > 0u) {
    _draw_blend_preview(motion, std::span<const std::float_t>{weights.data(), count});
  }

  auto removed = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < motion.children.size(); ++index) {
    auto& child = motion.children[index];

    ImGui::PushID(static_cast<int>(index));

    const auto name = child.kind == sbx::assets::animation_motion_kind::clip ? (child.clip_name.empty() ? std::string{"(no clip)"} : child.clip_name) : std::string{"Blend Tree"};
    const auto weight = index < count ? weights[index] : 0.0f;

    const auto open = ImGui::TreeNodeEx("##child", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "%zu: %s (%.2f)", index, name.c_str(), weight);

    ImGui::SameLine();

    if (ImGui::SmallButton(ICON_MDI_DELETE)) {
      removed = index;
    }

    if (open) {
      switch (motion.blend_type) {
        case sbx::assets::animation_blend_type::linear_1d: {
          changed |= ImGui::DragFloat("Threshold", &child.threshold, 0.01f);
          break;
        }
        case sbx::assets::animation_blend_type::freeform_directional_2d:
        case sbx::assets::animation_blend_type::freeform_cartesian_2d: {
          auto position = std::array<float, 2u>{child.position.x(), child.position.y()};

          if (ImGui::DragFloat2("Position", position.data(), 0.01f)) {
            child.position = sbx::math::vector2{position[0], position[1]};
            changed = true;
          }

          break;
        }
        case sbx::assets::animation_blend_type::direct: {
          changed |= _draw_float_parameter_combo("Weight Parameter", child.direct_parameter);
          break;
        }
      }

      if (index >= sbx::assets::max_blend_children) {
        ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f}, ICON_MDI_ALERT " Past the %zu-child limit: ignored at runtime.", sbx::assets::max_blend_children);
      }

      changed |= _draw_motion(child);

      ImGui::TreePop();
    }

    ImGui::PopID();
  }

  if (removed) {
    motion.children.erase(motion.children.begin() + static_cast<std::ptrdiff_t>(*removed));
    changed = true;
  }

  ImGui::BeginDisabled(motion.children.size() >= sbx::assets::max_blend_children);

  // A new child lands past the last one, so it doesn't stack onto an existing threshold or position.
  const auto add_child = [&](sbx::assets::animation_motion_kind kind) {
    auto child = sbx::assets::animation_motion{};
    child.kind = kind;

    if (!motion.children.empty()) {
      const auto& last = motion.children.back();
      child.threshold = last.threshold + 1.0f;
      child.position = last.position + sbx::math::vector2{1.0f, 0.0f};
    }

    motion.children.push_back(std::move(child));
    changed = true;
  };

  if (ImGui::Button(ICON_MDI_PLUS " Clip")) {
    add_child(sbx::assets::animation_motion_kind::clip);
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_MDI_PLUS " Blend Tree")) {
    add_child(sbx::assets::animation_motion_kind::blend);
  }

  ImGui::EndDisabled();

  return changed;
}

auto animation_graph_panel::_state_name(std::uint32_t state_id) const -> std::string {
  const auto entry = std::ranges::find(_layer().states, state_id, &sbx::assets::animation_state::id);
  return (entry != _layer().states.end()) ? entry->name : std::string{"(unknown)"};
}

auto animation_graph_panel::draw(editor_state& state) -> void {
  SBX_PROFILE_SCOPE("animation_graph_panel::draw");

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

  _draw_layers();

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

  // Editor-only: lets Clip Name list the preview mesh's real clips instead of free text.
  const auto current = _preview_mesh.is_valid() ? sbx::render::asset_picker_item{_preview_mesh->id(), relative_asset_path(assets_module, _preview_mesh->id())} : sbx::render::asset_picker_item{};

  const auto options = sbx::render::asset_picker_options{
    .kind = sbx::assets::asset_type::mesh,
    .allow_none = true,
  };

  const auto result = sbx::render::draw_asset_picker("##animation_graph_preview_mesh", current, {}, options);

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
    if (draw_text_field("##name", parameter.name)) {
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

    if (sbx::render::draw_animation_parameter_value("##value", parameter.default_value)) {
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

    // Drop conditions on the deleted parameter rather than leave them never matching.
    for (auto& layer : _edit.layers) {
      for (auto& transition : layer.transitions) {
        std::erase_if(transition.conditions, [&removed_name](const auto& condition) { return condition.parameter_name == removed_name; });
      }
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

auto animation_graph_panel::_draw_mask_editor(sbx::assets::animation_layer& layer) -> bool {
  auto changed = false;

  const auto skeleton = _preview_mesh.is_valid() ? _preview_mesh->skeleton() : sbx::assets::skeleton_handle{};

  if (!skeleton.is_valid() || !skeleton->is_valid()) {
    // Without a skeleton to pick from, the mask is a list of joint names.
    auto removed = std::optional<std::size_t>{};

    for (auto index = std::size_t{0u}; index < layer.mask.size(); ++index) {
      ImGui::PushID(static_cast<int>(index));

      changed |= draw_text_field("##joint", layer.mask[index]);

      ImGui::SameLine();

      if (ImGui::SmallButton(ICON_MDI_TRASH_CAN)) {
        removed = index;
      }

      ImGui::PopID();
    }

    if (removed) {
      layer.mask.erase(layer.mask.begin() + static_cast<std::ptrdiff_t>(*removed));
      changed = true;
    }

    if (ImGui::Button(ICON_MDI_PLUS " Joint")) {
      layer.mask.emplace_back();
      changed = true;
    }

    ImGui::TextDisabled("Set a Preview Mesh with a skeleton to pick joints from a tree.");

    return changed;
  }

  const auto& joints = skeleton->joints();

  auto children = std::vector<std::vector<std::size_t>>(joints.size());

  for (auto index = std::size_t{0u}; index < joints.size(); ++index) {
    if (joints[index].parent_index >= 0) {
      children[static_cast<std::size_t>(joints[index].parent_index)].push_back(index);
    }
  }

  // Ticking or unticking a joint does the same to everything below it, the usual way to mask a limb or the upper body.
  const auto set_subtree = [&](this const auto& self, std::size_t index, bool included) -> void {
    const auto& name = joints[index].name;
    const auto present = std::ranges::contains(layer.mask, name);

    if (included && !present) {
      layer.mask.push_back(name);
    } else if (!included && present) {
      std::erase(layer.mask, name);
    }

    for (const auto child : children[index]) {
      self(child, included);
    }
  };

  const auto draw_joint = [&](this const auto& self, std::size_t index) -> void {
    ImGui::PushID(static_cast<int>(index));

    auto included = std::ranges::contains(layer.mask, joints[index].name);

    if (ImGui::Checkbox("##included", &included)) {
      set_subtree(index, included);
      changed = true;
    }

    ImGui::SameLine();

    const auto flags = ImGuiTreeNodeFlags_DefaultOpen | (children[index].empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None);

    if (ImGui::TreeNodeEx("##joint", flags, "%s", joints[index].name.c_str())) {
      for (const auto child : children[index]) {
        self(child);
      }

      ImGui::TreePop();
    }

    ImGui::PopID();
  };

  if (ImGui::SmallButton("All")) {
    layer.mask.clear();

    for (const auto& joint : joints) {
      layer.mask.push_back(joint.name);
    }

    changed = true;
  }

  ImGui::SameLine();

  if (ImGui::SmallButton("None")) {
    layer.mask.clear();
    changed = true;
  }

  ImGui::SameLine();

  if (layer.mask.empty()) {
    ImGui::TextDisabled("(no mask: every joint)");
  } else {
    ImGui::TextDisabled("%zu joints", layer.mask.size());
  }

  ImGui::BeginChild("##mask_joints", ImVec2{0.0f, 220.0f}, ImGuiChildFlags_Borders);

  for (auto index = std::size_t{0u}; index < joints.size(); ++index) {
    if (joints[index].parent_index < 0) {
      draw_joint(index);
    }
  }

  ImGui::EndChild();

  return changed;
}

auto animation_graph_panel::_draw_layers() -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_LAYERS " Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  // The canvas shows one layer at a time; switching re-seeds node positions from that layer's states.
  // A switch made by a button (add, move, delete) also moves the tab bar's selection, which only takes effect next frame; until then the old tab's report is ignored.
  const auto select_layer = [&](std::size_t index, bool move_tab) {
    _select_layer_tab |= move_tab;

    if (index == _layer_index) {
      return;
    }

    _layer_index = index;
    _selection = std::monostate{};
    _seeded_positions.clear();
    _any_state_seeded = false;
  };

  if (ImGui::BeginTabBar("##animation_layers", ImGuiTabBarFlags_FittingPolicyScroll)) {
    const auto moving_tab = std::exchange(_select_layer_tab, false);

    for (auto index = std::size_t{0u}; index < _edit.layers.size(); ++index) {
      const auto label = fmt::format("{}###layer_{}", _edit.layers[index].name, index);
      const auto flags = (moving_tab && index == _layer_index) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;

      if (ImGui::BeginTabItem(label.c_str(), nullptr, flags)) {
        if (!moving_tab) {
          select_layer(index, false);
        }

        ImGui::EndTabItem();
      }
    }

    if (ImGui::TabItemButton(ICON_MDI_PLUS, ImGuiTabItemFlags_Trailing)) {
      auto layer = sbx::assets::animation_layer{};
      layer.name = fmt::format("Layer {}", _edit.layers.size());
      layer.states.push_back(sbx::assets::animation_state{.id = 0u, .name = "Empty"});

      _edit.layers.push_back(std::move(layer));
      select_layer(_edit.layers.size() - 1u, true);
      _apply_live();
    }

    ImGui::EndTabBar();
  }

  auto& layer = _layer();
  const auto is_base = _layer_index == 0u;

  if (draw_text_field("Layer Name", layer.name)) {
    _apply_live();
  }

  if (is_base) {
    ImGui::TextDisabled("The base layer always plays at full weight over every joint.");
    return;
  }

  if (ImGui::SliderFloat("Weight", &layer.weight, 0.0f, 1.0f)) {
    _apply_live();
  }

  static constexpr auto mode_labels = std::array<const char*, 2u>{"Override", "Additive"};

  auto mode = static_cast<int>(layer.blend_mode);

  if (ImGui::Combo("Blending", &mode, mode_labels.data(), static_cast<int>(mode_labels.size()))) {
    layer.blend_mode = static_cast<sbx::assets::animation_layer_blend_mode>(mode);
    _apply_live();
  }

  if (ImGui::TreeNodeEx("Mask", ImGuiTreeNodeFlags_None)) {
    if (_draw_mask_editor(layer)) {
      _apply_live();
    }

    ImGui::TreePop();
  }

  // Order matters: each layer blends over everything before it.
  ImGui::BeginDisabled(_layer_index <= 1u);

  if (ImGui::Button(ICON_MDI_ARROW_UP " Move Up")) {
    std::swap(_edit.layers[_layer_index], _edit.layers[_layer_index - 1u]);
    select_layer(_layer_index - 1u, true);
    _apply_live();
  }

  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(_layer_index + 1u >= _edit.layers.size());

  if (ImGui::Button(ICON_MDI_ARROW_DOWN " Move Down")) {
    std::swap(_edit.layers[_layer_index], _edit.layers[_layer_index + 1u]);
    select_layer(_layer_index + 1u, true);
    _apply_live();
  }

  ImGui::EndDisabled();
  ImGui::SameLine();

  if (ImGui::Button(ICON_MDI_TRASH_CAN " Delete Layer")) {
    _edit.layers.erase(_edit.layers.begin() + static_cast<std::ptrdiff_t>(_layer_index));
    select_layer(_layer_index - 1u, true);
    _apply_live();
  }
}

constexpr auto input_pin_color = IM_COL32(94, 174, 255, 255);
constexpr auto output_pin_color = IM_COL32(255, 176, 79, 255);

// A pin circle, filled when a transition is attached and hollow otherwise.
static auto draw_pin_icon(bool connected, ImU32 color) -> void {
  constexpr auto diameter = 11.0f;

  // The reserved box is a text line tall, centering the circle on the label's row.
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

  auto connected_inputs = std::unordered_set<std::uint32_t>{};
  auto connected_outputs = std::unordered_set<std::uint32_t>{};
  auto any_state_connected = false;

  for (const auto& transition : _layer().transitions) {
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

  for (auto& state : _layer().states) {
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

    if (state.id == _layer().entry_state_id) {
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

  for (auto index = std::size_t{0u}; index < _layer().transitions.size(); ++index) {
    const auto& transition = _layer().transitions[index];
    const auto source_pin = transition.from_state.has_value() ? output_pin_for_state(*transition.from_state) : any_state_output_pin();

    ax::NodeEditor::Link(link_id_for_transition(index), source_pin, input_pin_for_state(transition.to_state));
  }

  if (ax::NodeEditor::BeginCreate()) {
    auto start_pin = ax::NodeEditor::PinId{};
    auto end_pin = ax::NodeEditor::PinId{};

    if (ax::NodeEditor::QueryNewLink(&start_pin, &end_pin)) {
      const auto start = resolve_pin(start_pin);
      const auto end = resolve_pin(end_pin);

      // A drag can go either way; normalize by pin kind.
      const auto* source = start.is_output ? &start : (end.is_output ? &end : nullptr);
      const auto* target = start.is_output ? &end : (end.is_output ? &start : nullptr);

      if (source == nullptr || target == nullptr || target == source || target->is_output || target->is_any_state) {
        ax::NodeEditor::RejectNewItem(ImVec4{1.0f, 0.3f, 0.3f, 1.0f});
      } else if (ax::NodeEditor::AcceptNewItem()) {
        auto transition = sbx::assets::animation_transition{};
        transition.from_state = source->is_any_state ? std::nullopt : std::optional<std::uint32_t>{source->state_id};
        transition.to_state = target->state_id;

        _layer().transitions.push_back(transition);
        _selection = _layer().transitions.size() - 1u;
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
        if (*transition_index < _layer().transitions.size()) {
          _layer().transitions.erase(_layer().transitions.begin() + static_cast<std::ptrdiff_t>(*transition_index));
        }

        _selection = std::monostate{};
        _apply_live();
      }
    }

    auto node_id = ax::NodeEditor::NodeId{};

    while (ax::NodeEditor::QueryDeletedNode(&node_id)) {
      const auto deleted_state_id = state_id_from_node(node_id);

      // The Any State pseudo-node isn't part of the asset, so its deletion is never accepted.
      if (deleted_state_id.has_value() && ax::NodeEditor::AcceptDeletedItem()) {
        std::erase_if(_layer().states, [&deleted_state_id](const auto& s) { return s.id == *deleted_state_id; });
        std::erase_if(_layer().transitions, [&deleted_state_id](const auto& t) { return t.to_state == *deleted_state_id || t.from_state == *deleted_state_id; });

        if (_layer().entry_state_id == *deleted_state_id && !_layer().states.empty()) {
          _layer().entry_state_id = _layer().states.front().id;
        }

        _seeded_positions.erase(*deleted_state_id);
        _selection = std::monostate{};
        _apply_live();
      }
    }
  }

  ax::NodeEditor::EndDelete();

  const auto spawn_position = ImGui::GetMousePos(); // captured before Suspend, in screen space

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
      if (ImGui::MenuItem(ICON_MDI_STAR " Set as Entry State", nullptr, false, _layer().entry_state_id != *context_state_id)) {
        _layer().entry_state_id = *context_state_id;
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

      for (const auto& existing : _layer().states) {
        next_id = std::max(next_id, existing.id + 1u);
      }

      auto new_state = sbx::assets::animation_state{};
      new_state.id = next_id;
      new_state.name = fmt::format("State {}", next_id);
      new_state.editor_position = sbx::math::vector2{spawn_position.x, spawn_position.y};

      _layer().states.push_back(new_state);

      if (_layer().states.size() == 1u) {
        _layer().entry_state_id = new_state.id;
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
    const auto entry = std::ranges::find(_layer().states, state_id, &sbx::assets::animation_state::id);

    if (entry == _layer().states.end()) {
      _selection = std::monostate{};
      return;
    }

    auto& selected_state = *entry;

    ImGui::SeparatorText("State");

    if (draw_text_field("Name", selected_state.name)) {
      _apply_live();
    }

    if (ImGui::DragFloat("Speed", &selected_state.speed, 0.05f, 0.0f, 10.0f)) {
      _apply_live();
    }

    if (_draw_float_parameter_combo("Speed Multiplier", selected_state.speed_parameter)) {
      _apply_live();
    }

    if (ImGui::Checkbox("Loop", &selected_state.loop)) {
      _apply_live();
    }

    ImGui::SeparatorText("Motion");

    {
      static constexpr auto kind_labels = std::array<const char*, 2u>{"Clip", "Blend Tree"};

      auto kind = static_cast<int>(selected_state.motion.kind);

      if (ImGui::Combo("Type", &kind, kind_labels.data(), static_cast<int>(kind_labels.size()))) {
        selected_state.motion.kind = static_cast<sbx::assets::animation_motion_kind>(kind);
        _apply_live();
      }
    }

    if (_draw_motion(selected_state.motion)) {
      _apply_live();
    }

    ImGui::BeginDisabled(_layer().entry_state_id == selected_state.id);

    if (ImGui::Button(ICON_MDI_STAR " Set as Entry State")) {
      _layer().entry_state_id = selected_state.id;
      _apply_live();
    }

    ImGui::EndDisabled();
  } else if (std::holds_alternative<std::size_t>(_selection)) {
    const auto index = std::get<std::size_t>(_selection);

    if (index >= _layer().transitions.size()) {
      _selection = std::monostate{};
      return;
    }

    auto& transition = _layer().transitions[index];

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

    auto exit_time = transition.exit_time.value();

    if (ImGui::DragFloat("Exit Time", &exit_time, 0.01f, 0.0f, 1.0f)) {
      transition.exit_time = exit_time;
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
            condition.expected = sbx::render::default_for_same_alternative(_edit.parameters[p].default_value);
            _apply_live();
          }
        }

        ImGui::EndCombo();
      }

      const auto is_trigger = current_parameter_index >= 0 && std::holds_alternative<sbx::assets::animation_trigger>(_edit.parameters[static_cast<std::size_t>(current_parameter_index)].default_value);
      const auto is_bool = current_parameter_index >= 0 && std::holds_alternative<bool>(_edit.parameters[static_cast<std::size_t>(current_parameter_index)].default_value);

      if (!is_trigger) {
        if (is_bool) {
          // Only equality is meaningful for a bool.
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

        if (sbx::render::draw_animation_parameter_value("Value", condition.expected)) {
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
        .expected = sbx::render::default_for_same_alternative(_edit.parameters.front().default_value)
      });

      _apply_live();
    }

    ImGui::EndDisabled();

    ImGui::Separator();

    if (ImGui::Button(ICON_MDI_TRASH_CAN " Delete Transition")) {
      _layer().transitions.erase(_layer().transitions.begin() + static_cast<std::ptrdiff_t>(index));
      _selection = std::monostate{};
      _apply_live();
    }
  } else {
    ImGui::TextDisabled("Select a state or transition.");
  }
}

} // namespace editor
