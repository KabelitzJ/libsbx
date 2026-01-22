// SPDX-License-Identifier: MIT
#include <libsbx/editor/themes.hpp>

#include <cmath>
#include <ranges>

#include <range/v3/all.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/editor/bindings/imgui.hpp>

namespace sbx::editor {

themes::themes() {
  _themes["Dark"] = [this]() { set_dark_theme_colors(); };
  _themes["Modern Dark"] = [this]() { set_modern_dark_colors(); };
  _themes["Catpuccin Mocha"] = [this]() { set_catpuccin_mocha_colors(); };
  _themes["Bess Dark"] = [this]() { set_bess_dark_colors(); };
  _themes["Fluent UI"] = [this]() { set_fluent_ui_colors(); };
  _themes["Moonlight"] = [this]() { set_moonlight_colors(); };
  _themes["Blueish"] = [this]() { set_blueish_colors(); };
  _themes["Foo"] = [this]() { set_foo_colors(); };
  _themes["Bar"] = [this]() { set_bar_colors(); };
  _themes["NVPro"] = [this]() { set_nvpro(); };
 }

auto themes::apply_theme(const std::string& theme) -> void {
  if (_themes.find(theme) == _themes.end()) {
    return;
  }

  _active_theme = theme;
  std::invoke(_themes[theme]);
  apply_color_correction();
}

auto themes::add_theme(const std::string& name, const std::function<void()>& callback) -> void {
  _themes[name] = callback;
}

auto themes::get_themes() const -> std::vector<std::string> {
  return _themes | ranges::views::keys | ranges::to<std::vector>;
}

auto themes::get_active_theme() const -> const std::string& {
  return _active_theme;
}

auto themes::set_bess_dark_colors() -> void {
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  // Primary background
  colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);  // #131318
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f); // #131318

  colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);

  // Headers
  colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.40f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);

  // Buttons
  colors[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.32f, 0.40f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.38f, 0.50f, 1.00f);

  // Frame BG
  colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);

  // Tabs
  colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.50f, 1.00f);
  colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.25f, 0.38f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);

  // Title
  colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

  // Borders
  colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.25f, 0.50f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  // Text
  colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);

  // Highlights
  colors[ImGuiCol_CheckMark] = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.80f, 1.00f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.50f, 0.70f, 1.00f, 0.50f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.60f, 0.80f, 1.00f, 0.75f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.70f, 0.90f, 1.00f, 1.00f);

  // Scrollbar
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.50f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.55f, 1.00f);

  // Style tweaks
  style.WindowRounding = 5.0f;
  style.FrameRounding = 5.0f;
  style.GrabRounding = 5.0f;
  style.TabRounding = 5.0f;
  style.PopupRounding = 5.0f;
  style.ScrollbarRounding = 5.0f;
  style.WindowPadding = ImVec2(10, 10);
  style.FramePadding = ImVec2(6, 4);
  style.ItemSpacing = ImVec2(8, 6);
  style.PopupBorderSize = 0.f;
}

auto themes::set_catpuccin_mocha_colors() -> void {
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  // Base colors inspired by Catppuccin Mocha
  colors[ImGuiCol_Text] = ImVec4(0.90f, 0.89f, 0.88f, 1.00f);         // Latte
  colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.56f, 0.52f, 1.00f); // Surface2
  colors[ImGuiCol_WindowBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f);     // Base
  colors[ImGuiCol_ChildBg] = ImVec4(0.18f, 0.16f, 0.22f, 1.00f);      // Mantle
  colors[ImGuiCol_PopupBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f);      // Base
  colors[ImGuiCol_Border] = ImVec4(0.27f, 0.23f, 0.29f, 1.00f);       // Overlay0
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f);              // Crust
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.20f, 0.29f, 1.00f);       // Overlay1
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.22f, 0.31f, 1.00f);        // Overlay2
  colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.12f, 0.18f, 1.00f);              // Mantle
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.17f, 0.15f, 0.21f, 1.00f);        // Mantle
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.12f, 0.18f, 1.00f);     // Mantle
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.17f, 0.15f, 0.22f, 1.00f);            // Base
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f);          // Base
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f);        // Crust
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.24f, 0.20f, 0.29f, 1.00f); // Overlay1
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.26f, 0.22f, 0.31f, 1.00f);  // Overlay2
  colors[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.66f, 0.47f, 1.00f);            // Peach
  colors[ImGuiCol_SliderGrab] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);           // Lavender
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f);     // Pink
  colors[ImGuiCol_Button] = ImVec4(0.65f, 0.34f, 0.46f, 1.00f);               // Maroon
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.71f, 0.40f, 0.52f, 1.00f);        // Red
  colors[ImGuiCol_ButtonActive] = ImVec4(0.76f, 0.46f, 0.58f, 1.00f);         // Pink
  colors[ImGuiCol_Header] = ImVec4(0.65f, 0.34f, 0.46f, 1.00f);               // Maroon
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.71f, 0.40f, 0.52f, 1.00f);        // Red
  colors[ImGuiCol_HeaderActive] = ImVec4(0.76f, 0.46f, 0.58f, 1.00f);         // Pink
  colors[ImGuiCol_Separator] = ImVec4(0.27f, 0.23f, 0.29f, 1.00f);            // Overlay0
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.95f, 0.66f, 0.47f, 1.00f);     // Peach
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.95f, 0.66f, 0.47f, 1.00f);      // Peach
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);           // Lavender
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f);    // Pink
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.92f, 0.61f, 0.85f, 1.00f);     // Mauve
  colors[ImGuiCol_Tab] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f);                  // Crust
  colors[ImGuiCol_TabHovered] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);           // Lavender
  colors[ImGuiCol_TabActive] = ImVec4(0.76f, 0.46f, 0.58f, 1.00f);            // Pink
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.16f, 0.22f, 1.00f);         // Mantle
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f);   // Crust
  colors[ImGuiCol_DockingPreview] = ImVec4(0.95f, 0.66f, 0.47f, 0.70f);       // Peach
  colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);       // Base
  colors[ImGuiCol_PlotLines] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);            // Lavender
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f);     // Pink
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);        // Lavender
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f); // Pink
  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);        // Mantle
  colors[ImGuiCol_TableBorderStrong] = ImVec4(0.27f, 0.23f, 0.29f, 1.00f);    // Overlay0
  colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);     // Surface2
  colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);  // Surface0
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.82f, 0.61f, 0.85f, 0.35f); // Lavender
  colors[ImGuiCol_DragDropTarget] = ImVec4(0.95f, 0.66f, 0.47f, 0.90f); // Peach
  colors[ImGuiCol_NavHighlight] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);   // Lavender
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

  // Style adjustments
  style.WindowRounding = 6.0f;
  style.FrameRounding = 4.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 3.0f;
  style.ChildRounding = 4.0f;

  style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
  style.WindowPadding = ImVec2(8.0f, 8.0f);
  style.FramePadding = ImVec2(5.0f, 4.0f);
  style.ItemSpacing = ImVec2(6.0f, 6.0f);
  style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
  style.IndentSpacing = 22.0f;

  style.ScrollbarSize = 14.0f;
  style.GrabMinSize = 10.0f;

  style.AntiAliasedLines = true;
  style.AntiAliasedFill = true;
}

auto themes::set_modern_dark_colors() -> void {
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  // Base color scheme
  colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.11f, 0.94f);
  colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.19f, 0.20f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.21f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.28f, 0.29f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.33f, 0.34f, 0.35f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.41f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.35f, 0.40f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 0.80f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 0.80f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 0.80f);
  colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.33f, 0.67f, 1.00f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.33f, 0.67f, 1.00f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
  colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.48f, 0.69f, 1.00f);
  colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.38f, 0.59f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_DockingPreview] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
  colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
  colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
  colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
  colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
  colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.28f, 0.56f, 1.00f, 0.35f);
  colors[ImGuiCol_DragDropTarget] = ImVec4(0.28f, 0.56f, 1.00f, 0.90f);
  colors[ImGuiCol_NavHighlight] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

  // Style adjustments
  style.WindowRounding = 5.3f;
  style.FrameRounding = 2.3f;
  style.ScrollbarRounding = 0;

  style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
  style.WindowPadding = ImVec2(8.0f, 8.0f);
  style.FramePadding = ImVec2(5.0f, 5.0f);
  style.ItemSpacing = ImVec2(6.0f, 6.0f);
  style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
  style.IndentSpacing = 25.0f;
}

auto themes::set_dark_theme_colors() -> void {
  auto& colors = ImGui::GetStyle().Colors;
  
  colors[ImGuiCol_WindowBg] = ImVec4{0.1f, 0.105f, 0.11f, 1.0f};

  // Headers
  colors[ImGuiCol_Header] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
  colors[ImGuiCol_HeaderHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
  colors[ImGuiCol_HeaderActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

  // Buttons
  colors[ImGuiCol_Button] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
  colors[ImGuiCol_ButtonHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
  colors[ImGuiCol_ButtonActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

  // Frame BG
  colors[ImGuiCol_FrameBg] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
  colors[ImGuiCol_FrameBgHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
  colors[ImGuiCol_FrameBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

  // Tabs
  colors[ImGuiCol_Tab] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.3805f, 0.381f, 1.0f};
  colors[ImGuiCol_TabActive] = ImVec4{0.28f, 0.2805f, 0.281f, 1.0f};
  colors[ImGuiCol_TabUnfocused] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};

  // Title
  colors[ImGuiCol_TitleBg] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_TitleBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
}

auto themes::set_fluent_ui_colors() -> void {
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  // General window settings
  style.WindowRounding = 5.0f;
  style.FrameRounding = 5.0f;
  style.ScrollbarRounding = 5.0f;
  style.GrabRounding = 5.0f;
  style.TabRounding = 5.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;
  style.PopupRounding = 5.0f;

  // Setting the colors
  colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.f);
  colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

  // Accent colors changed to darker olive-green/grey shades
  colors[ImGuiCol_CheckMark] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);        // Dark gray for check marks
  colors[ImGuiCol_SliderGrab] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);       // Dark gray for sliders
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f); // Slightly lighter gray when active
  colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);           // Button background (dark gray)
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);    // Button hover state
  colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);     // Button active state
  colors[ImGuiCol_Header] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);           // Dark gray for menu headers
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);    // Slightly lighter on hover
  colors[ImGuiCol_HeaderActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);     // Lighter gray when active
  colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);        // Separators in dark gray
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Resize grips in dark gray
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
  colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);        // Tabs background
  colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f); // Darker gray on hover
  colors[ImGuiCol_TabActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
  colors[ImGuiCol_DockingPreview] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Docking preview in gray
  colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f); // Empty dock background
  // Additional styles
  style.FramePadding = ImVec2(8.0f, 4.0f);
  style.ItemSpacing = ImVec2(8.0f, 4.0f);
  style.IndentSpacing = 20.0f;
  style.ScrollbarSize = 16.0f;
}

auto themes::set_moonlight_colors() -> void {
  // Moonlight style by deathsu/madam-herta
  // https://github.com/Madam-Herta/Moonlight/
	ImGuiStyle& style = ImGui::GetStyle();
	
	style.Alpha = 1.0f;
	style.DisabledAlpha = 1.0f;
	style.WindowPadding = ImVec2(12.0f, 12.0f);
	style.WindowRounding = 11.5f;
	style.WindowBorderSize = 0.0f;
	style.WindowMinSize = ImVec2(20.0f, 20.0f);
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Right;
	style.ChildRounding = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(20.0f, 3.400000095367432f);
	style.FrameRounding = 11.89999961853027f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(4.300000190734863f, 5.5f);
	style.ItemInnerSpacing = ImVec2(7.099999904632568f, 1.799999952316284f);
	style.CellPadding = ImVec2(12.10000038146973f, 9.199999809265137f);
	style.IndentSpacing = 0.0f;
	style.ColumnsMinSpacing = 4.900000095367432f;
	style.ScrollbarSize = 11.60000038146973f;
	style.ScrollbarRounding = 15.89999961853027f;
	style.GrabMinSize = 3.700000047683716f;
	style.GrabRounding = 20.0f;
	style.TabRounding = 0.0f;
	style.TabBorderSize = 0.0f;
	style.TabMinWidthForCloseButton = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
	
	style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.2745098173618317f, 0.3176470696926117f, 0.4509803950786591f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.09250493347644806f, 0.100297249853611f, 0.1158798336982727f, 1.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.1120669096708298f, 0.1262156516313553f, 0.1545064449310303f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.09803921729326248f, 0.105882354080677f, 0.1215686276555061f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.9725490212440491f, 1.0f, 0.4980392158031464f, 1.0f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.971993625164032f, 1.0f, 0.4980392456054688f, 1.0f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.7953379154205322f, 0.4980392456054688f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.1821731775999069f, 0.1897992044687271f, 0.1974248886108398f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.1545050293207169f, 0.1545048952102661f, 0.1545064449310303f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.1414651423692703f, 0.1629818230867386f, 0.2060086131095886f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.1072951927781105f, 0.107295036315918f, 0.1072961091995239f, 1.0f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.1293079704046249f, 0.1479243338108063f, 0.1931330561637878f, 1.0f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.1568627506494522f, 0.1843137294054031f, 0.250980406999588f, 1.0f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.1568627506494522f, 0.1843137294054031f, 0.250980406999588f, 1.0f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.1459212601184845f, 0.1459220051765442f, 0.1459227204322815f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.9725490212440491f, 1.0f, 0.4980392158031464f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.999999463558197f, 1.0f, 0.9999899864196777f, 1.0f);
	style.Colors[ImGuiCol_Tab] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1249424293637276f, 0.2735691666603088f, 0.5708154439926147f, 1.0f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.5215686559677124f, 0.6000000238418579f, 0.7019608020782471f, 1.0f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.03921568766236305f, 0.9803921580314636f, 0.9803921580314636f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8841201663017273f, 0.7941429018974304f, 0.5615870356559753f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.9570815563201904f, 0.9570719599723816f, 0.9570761322975159f, 1.0f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.09803921729326248f, 0.105882354080677f, 0.1215686276555061f, 1.0f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.9356134533882141f, 0.9356129765510559f, 0.9356223344802856f, 1.0f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.4980392158031464f, 0.5137255191802979f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.266094446182251f, 0.2890366911888123f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.4980392158031464f, 0.5137255191802979f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.196078434586525f, 0.1764705926179886f, 0.5450980663299561f, 0.501960813999176f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.196078434586525f, 0.1764705926179886f, 0.5450980663299561f, 0.501960813999176f);
}

auto themes::set_blueish_colors() -> void {
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  ImVec4 dark_blue_bg        = ImVec4(0.11f, 0.15f, 0.17f, 1.00f); // background
  ImVec4 mid_blue            = ImVec4(0.20f, 0.26f, 0.30f, 1.00f); // panels
  ImVec4 highlight_blue      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f); // active
  ImVec4 text_color          = ImVec4(0.86f, 0.93f, 0.97f, 1.00f); // bright text
  ImVec4 disabled_text_color = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);

  style.WindowRounding    = 4.0f;
  style.FrameRounding     = 4.0f;
  style.ScrollbarRounding = 6.0f;
  style.GrabRounding      = 4.0f;
  style.FrameBorderSize   = 1.0f;
  style.WindowBorderSize  = 1.0f;
  style.WindowPadding     = ImVec2(10, 10);
  style.FramePadding      = ImVec2(6, 4);

  colors[ImGuiCol_Text]                   = text_color;
  colors[ImGuiCol_TextDisabled]           = disabled_text_color;
  colors[ImGuiCol_WindowBg]               = dark_blue_bg;
  colors[ImGuiCol_ChildBg]                = dark_blue_bg;
  colors[ImGuiCol_PopupBg]                = mid_blue;
  colors[ImGuiCol_Border]                 = ImVec4(0.15f, 0.20f, 0.25f, 1.00f);
  colors[ImGuiCol_FrameBg]                = mid_blue;
  colors[ImGuiCol_FrameBgHovered]         = highlight_blue;
  colors[ImGuiCol_FrameBgActive]          = highlight_blue;
  colors[ImGuiCol_TitleBg]                = mid_blue;
  colors[ImGuiCol_TitleBgActive]          = highlight_blue;
  colors[ImGuiCol_MenuBarBg]              = mid_blue;
  colors[ImGuiCol_ScrollbarBg]            = mid_blue;
  colors[ImGuiCol_ScrollbarGrab]          = highlight_blue;
  colors[ImGuiCol_ScrollbarGrabHovered]   = highlight_blue;
  colors[ImGuiCol_ScrollbarGrabActive]    = highlight_blue;
  colors[ImGuiCol_CheckMark]              = highlight_blue;
  colors[ImGuiCol_SliderGrab]             = highlight_blue;
  colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.37f, 0.74f, 1.00f, 1.00f);
  colors[ImGuiCol_Button]                 = mid_blue;
  colors[ImGuiCol_ButtonHovered]          = highlight_blue;
  colors[ImGuiCol_ButtonActive]           = ImVec4(0.20f, 0.52f, 0.85f, 1.00f);
  colors[ImGuiCol_Header]                 = mid_blue;
  colors[ImGuiCol_HeaderHovered]          = highlight_blue;
  colors[ImGuiCol_HeaderActive]           = highlight_blue;
  colors[ImGuiCol_Separator]              = ImVec4(0.15f, 0.20f, 0.25f, 1.00f);
  colors[ImGuiCol_ResizeGrip]             = highlight_blue;
  colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.37f, 0.74f, 1.00f, 1.00f);
  colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_Tab]                    = mid_blue;
  colors[ImGuiCol_TabHovered]             = highlight_blue;
  colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.52f, 0.85f, 1.00f);
  colors[ImGuiCol_TabUnfocused]           = mid_blue;
  colors[ImGuiCol_TabUnfocusedActive]     = highlight_blue;
}

auto themes::set_foo_colors() -> void {
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

	// Color palette
	ImVec4 _black = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	ImVec4 _white = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	ImVec4 _grey = ImVec4(0.60f, 0.60f, 0.60f, 0.35f);
	ImVec4 _dark = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	ImVec4 _darkgrey = ImVec4(0.23f, 0.23f, 0.23f, 0.35f);
	ImVec4 _lighgrey = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

	// Color array
	colors[ImGuiCol_Text] = _white;
	colors[ImGuiCol_TextDisabled] = _grey;
	colors[ImGuiCol_WindowBg] = _dark;
	colors[ImGuiCol_ChildBg] = _dark;
	colors[ImGuiCol_PopupBg] = _dark;
	colors[ImGuiCol_Border] = _grey;
	colors[ImGuiCol_BorderShadow] = _black;
	colors[ImGuiCol_FrameBg] = _darkgrey;
	colors[ImGuiCol_FrameBgHovered] = _grey;
	colors[ImGuiCol_FrameBgActive] = _grey;
	colors[ImGuiCol_TitleBg] = _darkgrey;
	colors[ImGuiCol_TitleBgActive] = _darkgrey;
	colors[ImGuiCol_TitleBgCollapsed] = _darkgrey;
	colors[ImGuiCol_MenuBarBg] = _darkgrey;
	colors[ImGuiCol_ScrollbarBg] = _darkgrey;
	colors[ImGuiCol_ScrollbarGrabHovered] = _grey;
	colors[ImGuiCol_ScrollbarGrabActive] = _grey;
	colors[ImGuiCol_CheckMark] = _lighgrey;
	colors[ImGuiCol_SliderGrab] = _lighgrey;
	colors[ImGuiCol_SliderGrabActive] = _white;
	colors[ImGuiCol_Button] = _darkgrey;
	colors[ImGuiCol_ButtonHovered] = _grey;
	colors[ImGuiCol_ButtonActive] = _darkgrey;
	colors[ImGuiCol_Header] = _darkgrey;
	colors[ImGuiCol_HeaderHovered] = _grey;
	colors[ImGuiCol_HeaderActive] = _grey;
	colors[ImGuiCol_Separator] = _grey;
	colors[ImGuiCol_SeparatorHovered] = _grey;
	colors[ImGuiCol_SeparatorActive] = _grey;
	colors[ImGuiCol_ResizeGrip] = _darkgrey;
	colors[ImGuiCol_ResizeGripHovered] = _grey;
	colors[ImGuiCol_ResizeGripActive] = _grey;
	colors[ImGuiCol_Tab] = _darkgrey;
	colors[ImGuiCol_TabHovered] = _grey;
	colors[ImGuiCol_TabActive] = _grey;
	colors[ImGuiCol_TabUnfocused] = _grey;
	colors[ImGuiCol_TabUnfocused] = _grey;
	colors[ImGuiCol_TabUnfocusedActive] = _grey;
	colors[ImGuiCol_DockingPreview] = _grey;
	colors[ImGuiCol_DockingEmptyBg] = _grey;
	colors[ImGuiCol_PlotLines] = _white;
	colors[ImGuiCol_PlotLinesHovered] = _grey;
	colors[ImGuiCol_PlotHistogram] = _white;
	colors[ImGuiCol_PlotHistogramHovered] = _grey;
	colors[ImGuiCol_TableHeaderBg] = _dark;
	colors[ImGuiCol_TableBorderStrong] = _darkgrey;
	colors[ImGuiCol_TableBorderLight] = _grey;
	colors[ImGuiCol_TableRowBg] = _black;
	colors[ImGuiCol_TableRowBgAlt] = _white;
	colors[ImGuiCol_TextSelectedBg] = _darkgrey;
	colors[ImGuiCol_DragDropTarget] = _darkgrey;
	colors[ImGuiCol_NavHighlight] = _grey;
	colors[ImGuiCol_NavWindowingHighlight] = _grey;
	colors[ImGuiCol_NavWindowingDimBg] = _grey;
	colors[ImGuiCol_ModalWindowDimBg] = _grey;

	// Style
	style.FrameRounding = 3;
	style.WindowPadding = ImVec2(10.0f, 10.0f);
	style.FramePadding = ImVec2(10.00f, 10.00f);
	style.CellPadding = ImVec2(10.00f, 5.00f);
	style.ItemSpacing = ImVec2(10.00f, 5.00f);
	style.ItemInnerSpacing = ImVec2(5.00f, 5.00f);
	style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
	style.IndentSpacing = 15;
	style.ScrollbarSize = 18;
	style.GrabMinSize = 10;
	style.WindowBorderSize = 0;
	style.ChildBorderSize = 0;
	style.PopupBorderSize = 0;
	style.FrameBorderSize = 0;
	style.TabBorderSize = 0;
	style.WindowRounding = 5;
	style.ChildRounding = 5;
	style.PopupRounding = 0;
	style.ScrollbarRounding = 5;
	style.GrabRounding = 0;
	style.LogSliderDeadzone = 0;
	style.TabRounding = 0;
	style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
	style.WindowMenuButtonPosition = ImGuiDir_None;
	style.ColorButtonPosition = ImGuiDir_Left;
}

static auto lerp(const ImVec4& a, const ImVec4& b, float t) -> ImVec4 {
  return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

static auto bg_color_1 = ImVec4{0.1f,0.1f,0.1f,1.0f};
static auto bg_color_2 = ImVec4{0.59f,0.59f,0.59f,1.0f};
static auto h_color_1  = ImVec4{1.0f,1.0f,1.0f,1.0f};
static auto h_color_2  = ImVec4{1.0f,1.0f,1.0f,0.1f};
static auto color_accent_1 = ImVec4{59.0f / 255.0f, 79.0f / 255.0f, 255.0f / 255.0f, 1.0f};
static auto color_accent_2 = ImVec4{45.0f / 255.0f, 80.0f / 255.0f, 255.0f / 255.0f, 1.0f};
static auto color_ok      = ImVec4{51.0f / 255.0f, 179.0f / 255.0f, 89.0f / 255.0f, 1.0f};
static auto color_info    = ImVec4{235.0f / 255.0f, 235.0f / 255.0f, 235.0f / 255.0f, 1.0f};
static auto color_warning = ImVec4{255.0f / 255.0f, 149.0f / 255.0f, 49.0f / 255.0f, 1.0f};
static auto color_error   = ImVec4{255.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f};

auto themes::set_bar_colors() -> void {
  auto& style = ImGui::GetStyle();

  bg_color_1 = ImVec4{30.0f / 255.0f, 30.0f / 255.0f, 41.0f / 255.0f, 1.0f};
  bg_color_2 = ImVec4{71.0f / 255.0f, 85.0f / 255.0f, 117.0f / 255.0f, 1.0f};
 
  h_color_1  = ImVec4{1.0,1.0,1.0,1.0f};
  h_color_2  = ImVec4{1.0,1.0,1.0,0.1f};
 
  color_accent_1 = ImVec4{181.0f / 255.0f, 198.0f / 255.0f, 238.0f / 255.0f, 1.0f};
  color_accent_2 = ImVec4{79.0f / 255.0f, 82.0f / 255.0f, 99.0f / 255.0f, 1.0f};
 
  color_ok        = ImVec4{51.0f / 255.0f, 179.0f / 255.0f, 89.0f / 255.0f, 1.0f};
  color_info      = ImVec4{235.0f / 255.0f, 235.0f / 255.0f, 235.0f / 255.0f, 1.0f};
  color_warning   = ImVec4{255.0f / 255.0f, 149.0f / 255.0f, 49.0f / 255.0f, 1.0f};
  color_error     = ImVec4{255.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f};

  style.Alpha                     = 1.0f;
  style.DisabledAlpha             = 0.60f;

  style.WindowPadding             = ImVec2(8.0f, 4.0f);
  style.CellPadding               = ImVec2(8.0f, 4.0f);
  style.FramePadding              = ImVec2(8.0f, 4.0f);
  style.ItemSpacing               = ImVec2(8.0f, 4.0f);

  style.WindowRounding            = 2.0f;
  style.GrabRounding              = 2.0f;
  style.TabRounding               = 2.0f;
  style.ChildRounding             = 2.0f;
  style.PopupRounding             = 2.0f;
  style.FrameRounding             = 2.0f;
  style.ScrollbarRounding         = 2.0f;

  style.WindowBorderSize          = 1.0f;
  style.PopupBorderSize           = 1.0f;

  style.ChildBorderSize           = 0.0f;
  style.FrameBorderSize           = 0.0f;
  style.TabBorderSize             = 0.0f;

  style.WindowMinSize             = ImVec2(32.0f, 32.0f);
  style.WindowTitleAlign          = ImVec2(0.0f, 0.5f);
  style.WindowMenuButtonPosition  = ImGuiDir_Left;

  style.ItemInnerSpacing          = ImVec2(2.0f, 2.0f);
  style.IndentSpacing             = 21.0f;
  style.ColumnsMinSpacing         = 6.0f;
  style.ScrollbarSize             = 13.0f;
  style.GrabMinSize               = 7.0f;
  style.TabMinWidthForCloseButton = 0.0f;
  style.ColorButtonPosition       = ImGuiDir_Right;
  style.ButtonTextAlign           = ImVec2(0.5f, 0.5f);
  style.SelectableTextAlign       = ImVec2(0.0f, 0.0f);

  auto color_background_1                        = lerp(bg_color_1, bg_color_2, .0f);
  auto color_background_2                        = lerp(bg_color_1, bg_color_2, .1f);
  auto color_background_3                        = lerp(bg_color_1, bg_color_2, .2f);
  auto color_background_4                        = lerp(bg_color_1, bg_color_2, .3f);
  auto color_background_5                        = lerp(bg_color_1, bg_color_2, .4f);
  auto color_background_6                        = lerp(bg_color_1, bg_color_2, .5f);
  auto color_background_7                        = lerp(bg_color_1, bg_color_2, .6f);
  auto color_background_8                        = lerp(bg_color_1, bg_color_2, .7f);
  auto color_background_9                        = lerp(bg_color_1, bg_color_2, .8f);
  auto color_background_10                       = lerp(bg_color_1, bg_color_2, .9f);

  // should be dark
  auto color_black_transparent_9                 = ImVec4{0.0f, 0.0f, 0.0f, 0.9f};
  auto color_black_transparent_6                 = ImVec4{0.0f, 0.0f, 0.0f, 0.6f};
  auto color_black_transparent_3                 = ImVec4{0.0f, 0.0f, 0.0f, 0.3f};
  auto color_black_transparent_1                 = ImVec4{0.0f, 0.0f, 0.0f, 0.1f};

  auto color_highlight_1                         = lerp(h_color_1,h_color_2, 0);

  auto color_accent_2                            = lerp(h_color_1,h_color_2, 0.2f);//{55.0f / 255.0f, 75.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  auto color_accent_3                            = lerp(h_color_1,h_color_2, 0.3f);//{50.0f / 255.0f, 70.0f / 255.0f, 255.0f / 255.0f, 1.0f};

  // not used
  // ImVec4 color_highlight_2                      = Lerp(h_color_1,h_color_2,.1);
  // ImVec4 color_highlight_3                      = Lerp(h_color_1,h_color_2,.2);
  // ImVec4 color_highlight_4                      = Lerp(h_color_1,h_color_2,.3);
  // ImVec4 color_highlight_5                      = Lerp(h_color_1,h_color_2,.4);
  // ImVec4 color_highlight_6                      = Lerp(h_color_1,h_color_2,.5);
  // ImVec4 color_highlight_7                      = Lerp(h_color_1,h_color_2,.6);
  // ImVec4 color_highlight_8                      = Lerp(h_color_1,h_color_2,.7);
  // ImVec4 color_highlight_9                      = Lerp(h_color_1,h_color_2,.8);
  // ImVec4 color_highlight_10                     = Lerp(h_color_1,h_color_2,.9);

  style.Colors[ImGuiCol_Text]                      = color_highlight_1;
  style.Colors[ImGuiCol_TextDisabled]              = color_background_9;

  style.Colors[ImGuiCol_WindowBg]                  = color_background_2;
  style.Colors[ImGuiCol_FrameBg]                   = color_background_4;
  style.Colors[ImGuiCol_TitleBg]                   = color_background_1;
  style.Colors[ImGuiCol_TitleBgActive]             = color_background_2;

  // accent
  style.Colors[ImGuiCol_ScrollbarGrabActive]       = color_accent_1;
  style.Colors[ImGuiCol_SeparatorActive]           = color_accent_1;
  style.Colors[ImGuiCol_SliderGrabActive]          = color_accent_1;
  style.Colors[ImGuiCol_ResizeGripActive]          = color_accent_1;
  style.Colors[ImGuiCol_DragDropTarget]            = color_accent_1;
  style.Colors[ImGuiCol_NavCursor]                 = color_accent_1;
  style.Colors[ImGuiCol_NavWindowingHighlight]     = color_accent_1;
  style.Colors[ImGuiCol_TabSelectedOverline]       = color_accent_1;
  style.Colors[ImGuiCol_TabDimmedSelectedOverline] = color_accent_1;
  style.Colors[ImGuiCol_CheckMark]                 = color_accent_1;

  style.Colors[ImGuiCol_Tab]                       = style.Colors[ImGuiCol_TitleBg];
  style.Colors[ImGuiCol_TabDimmed]                 = style.Colors[ImGuiCol_TitleBg];

  style.Colors[ImGuiCol_TabSelected]               = style.Colors[ImGuiCol_WindowBg];
  style.Colors[ImGuiCol_TabDimmedSelected]         = style.Colors[ImGuiCol_WindowBg];

  style.Colors[ImGuiCol_FrameBgHovered]            = color_background_3;

  style.Colors[ImGuiCol_TitleBgCollapsed]          = color_background_2;
  style.Colors[ImGuiCol_MenuBarBg]                 = color_background_3;
  style.Colors[ImGuiCol_ScrollbarBg]               = color_background_2;


  style.Colors[ImGuiCol_Button]                    = color_background_3;
  style.Colors[ImGuiCol_ButtonHovered]             = color_background_4;
  style.Colors[ImGuiCol_ButtonActive]              = color_background_1;

  // alternative
  // style.Colors[ImGuiCol_Button]                 = {};
  // style.Colors[ImGuiCol_ButtonHovered]          = color_highlight_4;
  // style.Colors[ImGuiCol_ButtonActive]           = color_highlight_5;

  style.Colors[ImGuiCol_ResizeGrip]                =  color_black_transparent_3;
  style.Colors[ImGuiCol_ResizeGripHovered]         = color_black_transparent_6;
  style.Colors[ImGuiCol_TableRowBgAlt]             = color_black_transparent_1;
  style.Colors[ImGuiCol_TextSelectedBg]            = color_black_transparent_1;

  style.Colors[ImGuiCol_DockingPreview]            = color_accent_1;
  style.Colors[ImGuiCol_PlotLinesHovered]          = color_accent_2;
  style.Colors[ImGuiCol_PlotHistogramHovered]      = color_accent_3;

  style.Colors[ImGuiCol_PlotHistogram]             = color_background_10;

  style.Colors[ImGuiCol_HeaderHovered]             = color_background_9;
  style.Colors[ImGuiCol_HeaderActive]              = color_background_9;
  style.Colors[ImGuiCol_PlotLines]                 = color_background_9;

  style.Colors[ImGuiCol_TabHovered]                = color_background_7;
  style.Colors[ImGuiCol_SeparatorHovered]          = color_background_8;
  style.Colors[ImGuiCol_SliderGrab]                = color_background_8;
  style.Colors[ImGuiCol_PopupBg]                   = color_background_6;
  style.Colors[ImGuiCol_Header]                    = color_background_6;
  style.Colors[ImGuiCol_TableBorderStrong]         = color_background_6;
  style.Colors[ImGuiCol_ScrollbarGrabHovered]      = color_background_6;
  style.Colors[ImGuiCol_Separator]                 = color_background_4;
  style.Colors[ImGuiCol_TableBorderLight]          = color_background_4;
  style.Colors[ImGuiCol_FrameBgActive]             = color_background_5;
  style.Colors[ImGuiCol_ScrollbarGrab]             = color_background_5;

  style.Colors[ImGuiCol_ChildBg]                   = {};
  style.Colors[ImGuiCol_Border]                    = color_background_5;

  style.Colors[ImGuiCol_TableHeaderBg]             = color_background_3;

  style.Colors[ImGuiCol_NavWindowingDimBg]         = color_black_transparent_6;
  style.Colors[ImGuiCol_ModalWindowDimBg]          = color_black_transparent_6;

  style.Colors[ImGuiCol_TableRowBg]                = {};
  style.Colors[ImGuiCol_BorderShadow]              = {};
}

auto themes::set_nvpro() -> void {
  ImGui::StyleColorsDark();

  ImGuiStyle& style                  = ImGui::GetStyle();
  style.WindowRounding               = 0.0f;
  style.WindowBorderSize             = 0.0f;
  style.ColorButtonPosition          = ImGuiDir_Right;
  style.FrameRounding                = 2.0f;
  style.FrameBorderSize              = 1.0f;
  style.GrabRounding                 = 4.0f;
  style.IndentSpacing                = 12.0f;
  style.Colors[ImGuiCol_WindowBg]    = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_MenuBarBg]   = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_PopupBg]     = ImVec4(0.135f, 0.135f, 0.135f, 1.0f);
  style.Colors[ImGuiCol_Border]      = ImVec4(0.4f, 0.4f, 0.4f, 0.5f);
  style.Colors[ImGuiCol_FrameBg]     = ImVec4(0.05f, 0.05f, 0.05f, 0.5f);

  // Normal
  ImVec4                normal_color = ImVec4(0.465f, 0.465f, 0.525f, 1.0f);
  std::vector<ImGuiCol> to_change_nrm;
  to_change_nrm.push_back(ImGuiCol_Header);
  to_change_nrm.push_back(ImGuiCol_SliderGrab);
  to_change_nrm.push_back(ImGuiCol_Button);
  to_change_nrm.push_back(ImGuiCol_CheckMark);
  to_change_nrm.push_back(ImGuiCol_ResizeGrip);
  to_change_nrm.push_back(ImGuiCol_TextSelectedBg);
  to_change_nrm.push_back(ImGuiCol_Separator);
  to_change_nrm.push_back(ImGuiCol_FrameBgActive);
  for(auto c : to_change_nrm)
  {
    style.Colors[c] = normal_color;
  }

  // Active
  ImVec4                active_color = ImVec4(0.365f, 0.365f, 0.425f, 1.0f);
  std::vector<ImGuiCol> to_change_act;
  to_change_act.push_back(ImGuiCol_HeaderActive);
  to_change_act.push_back(ImGuiCol_SliderGrabActive);
  to_change_act.push_back(ImGuiCol_ButtonActive);
  to_change_act.push_back(ImGuiCol_ResizeGripActive);
  to_change_act.push_back(ImGuiCol_SeparatorActive);
  for(auto c : to_change_act)
  {
    style.Colors[c] = active_color;
  }

  // Hovered
  ImVec4                hovered_color = ImVec4(0.565f, 0.565f, 0.625f, 1.0f);
  std::vector<ImGuiCol> to_change_hover;
  to_change_hover.push_back(ImGuiCol_HeaderHovered);
  to_change_hover.push_back(ImGuiCol_ButtonHovered);
  to_change_hover.push_back(ImGuiCol_FrameBgHovered);
  to_change_hover.push_back(ImGuiCol_ResizeGripHovered);
  to_change_hover.push_back(ImGuiCol_SeparatorHovered);
  for(auto c : to_change_hover)
  {
    style.Colors[c] = hovered_color;
  }


  style.Colors[ImGuiCol_TitleBgActive]    = ImVec4(0.465f, 0.465f, 0.465f, 1.0f);
  style.Colors[ImGuiCol_TitleBg]          = ImVec4(0.125f, 0.125f, 0.125f, 1.0f);
  style.Colors[ImGuiCol_Tab]              = ImVec4(0.05f, 0.05f, 0.05f, 0.5f);
  style.Colors[ImGuiCol_TabHovered]       = ImVec4(0.465f, 0.495f, 0.525f, 1.0f);
  style.Colors[ImGuiCol_TabActive]        = ImVec4(0.282f, 0.290f, 0.302f, 1.0f);
  style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.465f, 0.465f, 0.465f, 0.350f);

  //Colors_ext[ImGuiColExt_Warning] = ImVec4 (1.0f, 0.43f, 0.35f, 1.0f);

  ImGui::SetColorEditOptions(ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel);
}

auto themes::apply_color_correction() -> void {
  ImGuiStyle &style = ImGui::GetStyle();
  // Go through every colour and convert it to linear
  // This is because ImGui uses linear colours but we are using sRGB
  // This is a simple approximation of the conversion
  for (auto i = 0; i < ImGuiCol_COUNT; ++i) {
    // float linear = (ImVec4 <= 0.04045f) ? ImVec4 / 12.92f : pow((ImVec4 + 0.055f) * / 1.055f, 2.4f);

    ImVec4 &col = style.Colors[i];
    col.x = col.x <= 0.04045f ? col.x / 12.92f : std::pow((col.x + 0.055f) / 1.055f, 2.4f);
    col.y = col.y <= 0.04045f ? col.y / 12.92f : std::pow((col.y + 0.055f) / 1.055f, 2.4f);
    col.z = col.z <= 0.04045f ? col.z / 12.92f : std::pow((col.z + 0.055f) / 1.055f, 2.4f);
  }
}

} // namespace sbx::editor
