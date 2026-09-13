// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/inspector_component_registry.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>

#include <libsbx/canvas/components.hpp>

#include <editor/commands/component_commands.hpp>
#include <editor/commands/script_commands.hpp>

#include <editor/panels/inspector_component_sections.hpp>

namespace editor {

// Case-insensitive substring test, for the Add Component filter box and script-name matching below.
auto contains_ignore_case(std::string_view haystack, std::string_view needle) -> bool {
  const auto to_lower = [](std::string_view text) -> std::string {
    auto result = std::string{text};
    std::ranges::transform(result, result.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
  };

  return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
}

// Every script class name matching `filter` (case-insensitive substring) -- used only to decide
// whether the Script submenu below has anything to show for the active filter, so it can hide
// itself along with every other entry instead of opening onto an empty list.
auto any_script_matches(sbx::scripting::scripting_module& scripting_module, std::string_view filter) -> bool {
  auto& behavior_type = scripting_module.game_assembly().get_type("Sbx.Core.Behavior");

  for (auto* candidate : scripting_module.game_assembly().get_types()) {
    if (*candidate == behavior_type || !candidate->is_subclass_of(behavior_type)) {
      continue; // skip the base class itself and anything that isn't a Behavior
    }

    if (contains_ignore_case(std::string{candidate->get_full_name()}, filter)) {
      return true;
    }
  }

  return false;
}

template<typename Component>
auto make_entry(const char* icon, const char* name, const char* category, int exclusion_group, void (*draw_fn)(editor_state&, sbx::scenes::scene&, sbx::scenes::node&, sbx::assets::assets_module&)) -> component_entry {
  return component_entry{
    name, icon, category, exclusion_group,
    [](const sbx::scenes::node& node) { return node.has_component<Component>(); },
    [](editor_state& state, sbx::scenes::scene& target, const sbx::scenes::node& node, const std::string& label) {
      state.push_command(target, std::make_unique<add_component_command<Component>>(node.id(), label));
    },
    draw_fn
  };
}

// Collider kinds and layout-group kinds are each mutually exclusive -- narrowphase only ever
// resolves one collider per node (see narrowphase.cpp's resolve_convex), and a node with more than
// one layout group has no well-defined layout order.
constexpr auto collider_group = 1;
constexpr auto layout_group_group = 2;

auto component_entries() -> const std::vector<component_entry>& {
  static const auto entries = std::vector<component_entry>{
    make_entry<sbx::scenes::camera>(ICON_MDI_CAMERA_OUTLINE, "Camera", "Common", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_camera_section(s, t, n); }),

    make_entry<sbx::scenes::mesh_renderer>(ICON_MDI_CUBE_OUTLINE, "Mesh Renderer", "3D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module& a) { draw_mesh_renderer_section(s, t, n, a); }),

    // skeleton_pose isn't listed here -- it's fully auto-managed by draw_mesh_renderer_section
    // (see scenes::skeleton_pose's doc comment).
    make_entry<sbx::scenes::animator>(ICON_MDI_ANIMATION_PLAY, "Animator", "3D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_animator_section(s, t, n); }),

    make_entry<sbx::scenes::directional_light>(ICON_MDI_WHITE_BALANCE_SUNNY, "Directional Light", "3D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_directional_light_section(s, t, n); }),

    make_entry<sbx::scenes::point_light>(ICON_MDI_LIGHTBULB_OUTLINE, "Point Light", "3D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_point_light_section(s, t, n); }),

    make_entry<sbx::scenes::spot_light>(ICON_MDI_FLASHLIGHT, "Spot Light", "3D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_spot_light_section(s, t, n); }),

    make_entry<sbx::scenes::skybox>(ICON_MDI_EARTH, "Skybox", "3D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module& a) { draw_skybox_section(s, t, n, a); }),

    make_entry<sbx::scenes::particle_effect>(ICON_MDI_FIREWORK, "Particle Effect", "3D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module& a) { draw_particle_effect_instance_section(s, t, n, a); }),

    make_entry<sbx::physics::rigidbody>(ICON_MDI_SOCCER, "Rigidbody", "3D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_rigidbody_section(s, t, n); }),

    make_entry<sbx::physics::nav_agent>(ICON_MDI_WALK, "Nav Agent", "3D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_nav_agent_section(s, t, n); }),

    make_entry<sbx::physics::shape_collider>(ICON_MDI_SHAPE_OUTLINE, "Shape Collider", "3D", collider_group,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_shape_collider_section(s, t, n); }),

    make_entry<sbx::physics::mesh_collider>(ICON_MDI_TERRAIN, "Mesh Collider", "3D", collider_group,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module& a) { draw_mesh_collider_section(s, t, n, a); }),

    make_entry<sbx::canvas::canvas>(ICON_MDI_MONITOR, "Canvas", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_canvas_section(s, t, n); }),

    make_entry<sbx::canvas::canvas_group>(ICON_MDI_OPACITY, "Canvas Group", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_canvas_group_section(s, t, n); }),

    make_entry<sbx::canvas::canvas_scaler>(ICON_MDI_FIT_TO_SCREEN, "Canvas Scaler", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_canvas_scaler_section(s, t, n); }),

    make_entry<sbx::canvas::rect_transform>(ICON_MDI_ASPECT_RATIO, "Rect Transform", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_rect_transform_section(s, t, n); }),

    make_entry<sbx::canvas::ui_image>(ICON_MDI_IMAGE, "UI Image", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_ui_image_section(s, t, n); }),

    make_entry<sbx::canvas::ui_text>(ICON_MDI_FORMAT_TEXT, "UI Text", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_ui_text_section(s, t, n); }),

    make_entry<sbx::canvas::ui_button>(ICON_MDI_GESTURE_TAP_BUTTON, "UI Button", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_ui_button_section(s, t, n); }),

    make_entry<sbx::canvas::ui_toggle>(ICON_MDI_TOGGLE_SWITCH, "UI Toggle", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_ui_toggle_section(s, t, n); }),

    make_entry<sbx::canvas::ui_slider>(ICON_MDI_TUNE, "UI Slider", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_ui_slider_section(s, t, n); }),

    make_entry<sbx::canvas::ui_scrollbar>(ICON_MDI_DRAG_HORIZONTAL, "UI Scrollbar", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_ui_scrollbar_section(s, t, n); }),

    make_entry<sbx::canvas::ui_scroll_rect>(ICON_MDI_ARROW_ALL, "UI Scroll Rect", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_ui_scroll_rect_section(s, t, n); }),

    make_entry<sbx::canvas::layout_element>(ICON_MDI_RULER, "Layout Element", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_layout_element_section(s, t, n); }),

    make_entry<sbx::canvas::content_size_fitter>(ICON_MDI_ARROW_COLLAPSE_ALL, "Content Size Fitter", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_content_size_fitter_section(s, t, n); }),

    make_entry<sbx::canvas::horizontal_layout_group>(ICON_MDI_VIEW_COLUMN, "Horizontal Layout Group", "2D", layout_group_group,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_horizontal_layout_group_section(s, t, n); }),

    make_entry<sbx::canvas::vertical_layout_group>(ICON_MDI_VIEW_STREAM, "Vertical Layout Group", "2D", layout_group_group,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_vertical_layout_group_section(s, t, n); }),

    make_entry<sbx::canvas::grid_layout_group>(ICON_MDI_VIEW_GRID, "Grid Layout Group", "2D", layout_group_group,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_grid_layout_group_section(s, t, n); }),

    make_entry<sbx::canvas::ui_mask>(ICON_MDI_CROP, "UI Mask", "2D", 0,
      [](editor_state& s, sbx::scenes::scene& t, sbx::scenes::node& n, sbx::assets::assets_module&) { draw_ui_mask_section(s, t, n); }),
  };

  return entries;
}

auto draw_add_component_menu(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::scripting::scripting_module& scripting_module) -> void {
  static constexpr auto label = ICON_MDI_PLUS " Add Component";

  // Centered horizontally in the panel, rather than left-aligned like a regular control.
  const auto button_width = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
  const auto available_width = ImGui::GetContentRegionAvail().x;

  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (available_width - button_width) * 0.5f));

  if (ImGui::Button(label)) {
    ImGui::OpenPopup("##add_component_popup");
  }

  // Anchored under the button (centered, not wherever the mouse clicked) with a fixed width so it
  // doesn't jump or resize as the filter text changes. Must be set every frame the popup could
  // open -- ImGui only applies a pending SetNextWindowPos/Size to the very next Begin call.
  constexpr auto popup_width = 260.0f;
  const auto button_min = ImGui::GetItemRectMin();
  const auto button_max = ImGui::GetItemRectMax();
  const auto button_center_x = (button_min.x + button_max.x) * 0.5f;

  // Opens upward instead whenever there isn't roughly enough room below in the viewport's work
  // area, so a tall Inspector doesn't push the popup off the bottom of the window. The estimate
  // only has to pick the right side -- the upward case's pivot anchors to the popup's real height.
  constexpr auto estimated_popup_height = 320.0f;

  const auto work_min = ImGui::GetMainViewport()->WorkPos;
  const auto work_max = ImVec2{work_min.x + ImGui::GetMainViewport()->WorkSize.x, work_min.y + ImGui::GetMainViewport()->WorkSize.y};

  const auto opens_upward = (button_max.y + estimated_popup_height) > work_max.y;
  const auto popup_x = std::clamp(button_center_x - popup_width * 0.5f, work_min.x, std::max(work_min.x, work_max.x - popup_width));

  if (opens_upward) {
    // pivot {0, 1}: the given pos is the window's bottom-left corner instead of its top-left, so it
    // grows upward from the button using its own true content height, not the estimate above.
    ImGui::SetNextWindowPos(ImVec2{popup_x, button_min.y}, ImGuiCond_Always, ImVec2{0.0f, 1.0f});
  } else {
    ImGui::SetNextWindowPos(ImVec2{popup_x, button_max.y}, ImGuiCond_Always);
  }

  ImGui::SetNextWindowSize(ImVec2{popup_width, 0.0f}, ImGuiCond_Always);

  if (ImGui::BeginPopup("##add_component_popup")) {
    // A single always-the-same popup id, so IsWindowAppearing() alone tells us this is a fresh
    // open -- reset the filter and refocus it each time.
    static auto filter_buffer = std::array<char, 128u>{};

    if (ImGui::IsWindowAppearing()) {
      filter_buffer[0] = '\0';
      ImGui::SetKeyboardFocusHere();
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##add_component_filter", ICON_MDI_MAGNIFY " Filter components...", filter_buffer.data(), filter_buffer.size());

    const auto passes = [&](std::string_view name) {
      return filter_buffer[0] == '\0' || contains_ignore_case(name, filter_buffer.data());
    };

    ImGui::Separator();

    const auto& entries = component_entries();

    const auto blocked_by_exclusion = [&](const component_entry& entry) {
      if (entry.exclusion_group == 0) {
        return false;
      }

      return std::ranges::any_of(entries, [&](const component_entry& other) {
        return &other != &entry && other.exclusion_group == entry.exclusion_group && other.has(node);
      });
    };

    // Grouped into Common/3D/2D so the popup stays scannable as the component list grows -- a
    // category header only appears at all when something inside it would pass the current filter,
    // same "hide empty groups" reasoning the Script section below already uses.
    static constexpr auto categories = std::array<std::pair<const char*, const char*>, 3u>{{
      {ICON_MDI_PUZZLE_OUTLINE " Common", "Common"},
      {ICON_MDI_AXIS_ARROW " 3D", "3D"},
      {ICON_MDI_VECTOR_SQUARE " 2D", "2D"},
    }};

    for (const auto& [menu_label, category] : categories) {
      const auto category_visible = std::ranges::any_of(entries, [&](const component_entry& entry) {
        return std::string_view{entry.category} == category && passes(entry.name);
      });

      if (!category_visible) {
        continue;
      }

      if (ImGui::BeginMenu(menu_label)) {
        for (const auto& entry : entries) {
          if (std::string_view{entry.category} != category) {
            continue;
          }

          if (entry.has(node) || blocked_by_exclusion(entry) || !passes(entry.name)) {
            continue;
          }

          const auto item_label = fmt::format("{} {}", entry.icon, entry.name);

          if (ImGui::MenuItem(item_label.c_str())) {
            entry.add(state, target, node, fmt::format("Add {}", entry.name));
          }
        }

        ImGui::EndMenu();
      }
    }

    // Open-ended category, not a fixed name -- passes when "Script" matches or any script inside
    // it would, so a search for a script's own name doesn't hide the one submenu that satisfies it.
    if (passes("Script") || (filter_buffer[0] != '\0' && any_script_matches(scripting_module, filter_buffer.data()))) {
      if (ImGui::BeginMenu(ICON_MDI_FILE_CODE_OUTLINE " Script")) {
        auto& behavior_type = scripting_module.game_assembly().get_type("Sbx.Core.Behavior");
        auto any_available = false;

        for (auto* candidate : scripting_module.game_assembly().get_types()) {
          if (*candidate == behavior_type || !candidate->is_subclass_of(behavior_type)) {
            continue; // skip the base class itself and anything that isn't a Behavior
          }

          const auto full_name = std::string{candidate->get_full_name()};

          if (!passes(full_name)) {
            continue;
          }

          any_available = true;

          if (ImGui::MenuItem(full_name.c_str())) {
            state.push_command(target, std::make_unique<attach_script_command>(node.id(), full_name));
          }
        }

        if (!any_available) {
          ImGui::TextDisabled(filter_buffer[0] != '\0' ? "No scripts match." : "No compiled scripts found.");
        }

        ImGui::EndMenu();
      }
    }

    ImGui::EndPopup();
  }
}

} // namespace editor
