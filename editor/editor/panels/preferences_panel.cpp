// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/preferences_panel.hpp>

#include <libsbx/utility/profiler.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/reflection/enum.hpp>

#include <editor/editor_module.hpp>
#include <editor/widgets/text_field.hpp>

namespace editor {

// Whether two actions' shortcuts can collide: the same context, or one is global, or the Viewport's tools against scene editing (both live while the Viewport has focus).
auto contexts_overlap(action_context lhs, action_context rhs) -> bool {
  const auto is_scene_side = [](action_context context) { return context == action_context::scene || context == action_context::viewport; };

  return lhs == rhs || lhs == action_context::global || rhs == action_context::global || (is_scene_side(lhs) && is_scene_side(rhs));
}

// What the keyboard did this frame while recording: a chord (modifiers plus one key), Escape to cancel, or nothing yet.
struct recorded_chord {
  bool cancelled{false};
  std::optional<ImGuiKeyChord> chord{};
}; // struct recorded_chord

auto record_chord() -> recorded_chord {
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    return recorded_chord{true, std::nullopt};
  }

  for (auto key = static_cast<int>(ImGuiKey_NamedKey_BEGIN); key < static_cast<int>(ImGuiKey_NamedKey_END); ++key) {
    const auto named = static_cast<ImGuiKey>(key);

    // Modifiers only combine with a key; mouse buttons, wheels and gamepads aren't shortcuts.
    if ((named >= ImGuiKey_LeftCtrl && named <= ImGuiKey_RightSuper) || (named >= ImGuiKey_GamepadStart && named <= ImGuiKey_GamepadRStickDown) || (named >= ImGuiKey_MouseLeft && named <= ImGuiKey_MouseWheelY) || named >= ImGuiKey_ReservedForModCtrl) {
      continue;
    }

    if (ImGui::IsKeyPressed(named, false)) {
      return recorded_chord{false, static_cast<ImGuiKeyChord>(ImGui::GetIO().KeyMods) | named};
    }
  }

  return recorded_chord{};
}

auto preferences_panel::_save_shortcuts(editor_state& state) -> void {
  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();
  auto& overrides = editor_module.preferences().shortcut_overrides;

  overrides.clear();

  for (const auto& [id, chords] : state.actions.overrides()) {
    auto names = std::vector<std::string>{};

    for (const auto chord : chords) {
      names.push_back(chord_name(chord));
    }

    overrides.emplace(id, std::move(names));
  }

  editor_module.save_preferences();
}

auto preferences_panel::_draw_shortcuts(editor_state& state) -> void {
  auto& actions = state.actions;

  ImGui::SetNextItemWidth(-1.0f);
  draw_text_field_with_hint("##shortcut_filter", ICON_MDI_MAGNIFY " Filter actions", _shortcut_filter);

  // A recording in progress takes the next chord; dispatch is suspended meanwhile so it doesn't also run.
  if (!_recording_id.empty()) {
    if (const auto recorded = record_chord(); recorded.cancelled || recorded.chord) {
      if (recorded.chord) {
        if (const auto action = actions.find(_recording_id)) {
          const auto current = actions.chords_of(*action);
          auto chords = std::vector<ImGuiKeyChord>{current.begin(), current.end()};

          if (_recording_slot < chords.size()) {
            chords[_recording_slot] = *recorded.chord;
          } else {
            chords.push_back(*recorded.chord);
          }

          actions.set_override(_recording_id, std::move(chords));
          _save_shortcuts(state);
        }
      }

      _recording_id.clear();
    }
  }

  actions.set_capturing(!_recording_id.empty());

  if (!ImGui::BeginTable("##shortcuts", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY, ImVec2{0.0f, 0.0f})) {
    return;
  }

  ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 0.45f);
  ImGui::TableSetupColumn("Shortcuts", ImGuiTableColumnFlags_WidthStretch, 0.55f);
  ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableHeadersRow();

  const auto matches_filter = [this](const std::string& text) {
    return _shortcut_filter.empty() || !std::ranges::search(text, _shortcut_filter, [](char lhs, char rhs) { return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs)); }).empty();
  };

  auto category = std::string{};
  auto pending_reset = std::optional<std::string>{};
  auto pending_removal = std::optional<std::pair<std::string, std::size_t>>{};

  for (const auto& action : actions.actions()) {
    if (!matches_filter(fmt::format("{} {}", action.category, action.label))) {
      continue;
    }

    if (action.category != category) {
      category = action.category;

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("%s", category.c_str());
    }

    ImGui::PushID(action.id.c_str());

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s %s", action.icon, action.label.c_str());

    ImGui::TableSetColumnIndex(1);

    const auto chords = actions.chords_of(action);

    for (auto slot = std::size_t{0u}; slot <= chords.size(); ++slot) {
      ImGui::PushID(static_cast<int>(slot));

      const auto is_recording = _recording_id == action.id && _recording_slot == slot;
      const auto label = is_recording ? std::string{"Press keys..."} : slot < chords.size() ? chord_name(chords[slot]) : std::string{ICON_MDI_PLUS};

      if (slot > 0u) {
        ImGui::SameLine();
      }

      if (ImGui::SmallButton(label.c_str())) {
        _recording_id = action.id;
        _recording_slot = slot;
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", slot < chords.size() ? "Click to change, right-click to remove. Escape cancels." : "Add a shortcut.");
      }

      if (slot < chords.size() && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        pending_removal = std::pair{action.id, slot};
      }

      // Another action answering the same chord where both can be live.
      if (slot < chords.size()) {
        const auto chord = chords[slot];

        for (const auto& other : actions.actions()) {
          if (other.id != action.id && contexts_overlap(action.context, other.context) && std::ranges::contains(actions.chords_of(other), chord)) {
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::TextColored(ImVec4{1.0f, 0.75f, 0.2f, 1.0f}, ICON_MDI_ALERT);

            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Also used by %s: %s", other.category.c_str(), other.label.c_str());
            }

            break;
          }
        }
      }

      ImGui::PopID();
    }

    ImGui::TableSetColumnIndex(2);

    ImGui::BeginDisabled(!actions.overrides().contains(action.id));

    if (ImGui::SmallButton(ICON_MDI_RESTORE)) {
      pending_reset = action.id;
    }

    ImGui::EndDisabled();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Reset to the default shortcuts.");
    }

    ImGui::PopID();
  }

  ImGui::EndTable();

  if (pending_reset) {
    actions.clear_override(*pending_reset);
    _save_shortcuts(state);
  }

  if (pending_removal) {
    if (const auto action = actions.find(pending_removal->first)) {
      const auto current = actions.chords_of(*action);
      auto chords = std::vector<ImGuiKeyChord>{current.begin(), current.end()};
      chords.erase(chords.begin() + static_cast<std::ptrdiff_t>(pending_removal->second));

      actions.set_override(pending_removal->first, std::move(chords));
      _save_shortcuts(state);
    }
  }
}

auto preferences_panel::draw(editor_state& state) -> void {
  SBX_PROFILE_SCOPE("preferences_panel::draw");

  // Closed or collapsed mid-recording: shortcuts must work again.
  const auto stop_recording = [&] {
    state.actions.set_capturing(false);
    _recording_id.clear();
  };

  if (!is_open) {
    stop_recording();
    return;
  }

  ImGui::SetNextWindowSize(ImVec2{520.0f, 560.0f}, ImGuiCond_FirstUseEver);

  if (!ImGui::Begin(window_name, &is_open)) {
    stop_recording();
    ImGui::End();
    return;
  }

  if (ImGui::BeginTabBar("##preferences_tabs")) {
    if (ImGui::BeginTabItem("General")) {
      _draw_general();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Shortcuts")) {
      _draw_shortcuts(state);
      ImGui::EndTabItem();
    } else {
      stop_recording();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}

auto preferences_panel::_draw_general() -> void {
  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();
  auto& preferences = editor_module.preferences();
  auto changed = false;

  ImGui::SeparatorText("Interface");

  auto follow_monitor = preferences.interface_scale <= 0.0f;

  if (ImGui::Checkbox("Follow Monitor Scale", &follow_monitor)) {
    preferences.interface_scale = follow_monitor ? 0.0f : effective_interface_scale();
    changed = true;
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Sizes the editor UI by the monitor's content scale, for high-DPI screens.");
  }

  ImGui::BeginDisabled(follow_monitor);

  auto scale = effective_interface_scale();

  if (ImGui::SliderFloat("Interface Scale", &scale, 0.75f, 3.0f, "%.2fx")) {
    preferences.interface_scale = scale;
  }

  changed |= ImGui::IsItemDeactivatedAfterEdit();

  ImGui::EndDisabled();

  ImGui::SeparatorText("Colors");

  if (ImGui::BeginCombo("Axis Colors", std::string{sbx::reflection::to_string(preferences.axis_colors)}.c_str())) {
    for (const auto scheme : sbx::reflection::enum_values<axis_color_scheme>()) {
      if (ImGui::Selectable(std::string{sbx::reflection::to_string(scheme)}.c_str(), scheme == preferences.axis_colors)) {
        preferences.axis_colors = scheme;
        changed = true;
      }
    }

    ImGui::EndCombo();
  }

  ImGui::SeparatorText("Snapping (hold Ctrl while dragging a gizmo)");

  ImGui::DragFloat("Move Step", &preferences.translate_snap, 0.01f, 0.001f, 100.0f, "%.3f");
  changed |= ImGui::IsItemDeactivatedAfterEdit();
  ImGui::DragFloat("Rotate Step", &preferences.rotate_snap, 0.5f, 0.1f, 180.0f, "%.1f deg");
  changed |= ImGui::IsItemDeactivatedAfterEdit();
  ImGui::DragFloat("Scale Step", &preferences.scale_snap, 0.005f, 0.001f, 10.0f, "%.3f");
  changed |= ImGui::IsItemDeactivatedAfterEdit();

  if (changed) {
    editor_module.save_preferences();
  }

  // Saved with the camera's own camera.yaml when the editor closes -- see editor_camera.
  ImGui::SeparatorText("Editor Camera");

  auto& camera = editor_module.editor_camera();

  auto move_speed = camera.move_speed();

  if (ImGui::DragFloat("Move Speed", &move_speed, 0.1f, 0.1f, 500.0f, "%.1f")) {
    camera.set_move_speed(move_speed);
  }

  auto look_sensitivity = camera.look_sensitivity();

  if (ImGui::DragFloat("Look Sensitivity", &look_sensitivity, 0.0001f, 0.0001f, 0.05f, "%.4f")) {
    camera.set_look_sensitivity(look_sensitivity);
  }

  ImGui::DragFloat("Field of View", &camera.params().fov_degrees, 0.5f, 10.0f, 150.0f, "%.1f deg");
  ImGui::DragFloat("Near Plane", &camera.params().near_plane, 0.01f, 0.001f, camera.params().far_plane, "%.3f");
  ImGui::DragFloat("Far Plane", &camera.params().far_plane, 1.0f, camera.params().near_plane, 100000.0f, "%.1f");
}

} // namespace editor
