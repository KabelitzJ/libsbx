// SPDX-License-Identifier: MIT
#ifndef LIBSBX_EDITOR_MENU_BAR_HPP_
#define LIBSBX_EDITOR_MENU_BAR_HPP_

#include <string>
#include <functional>

#include <libsbx/editor/bindings/imgui.hpp>

namespace sbx::editor {

struct menu_item {
  std::string title;
  std::string short_cut;
  bool separator_after;
  std::function<void()> on_click;
}; // struct menu_item

struct menu {
  std::string title;
  bool separator_after;
  std::vector<menu> submenus;
  std::vector<menu_item> items;
}; // struct menu

auto add_menu_item(const menu_item& item) -> void {
  if (ImGui::MenuItem(item.title.c_str(), !item.short_cut.empty() ? item.short_cut.c_str() : nullptr)) {
    std::invoke(item.on_click);
  }

  if (item.separator_after) {
    ImGui::Separator();
  }
}

auto add_menu(const menu& menu) -> void {
  if (ImGui::BeginMenu(menu.title.c_str())) {
    for (const auto& submenu : menu.submenus) {
      add_menu(submenu);
    }

    for (const auto& item : menu.items) {
      add_menu_item(item);
    }

    ImGui::EndMenu();
  }

  if (menu.separator_after) {
    ImGui::Separator();
  }
}

auto add_menu_bar(const std::vector<menu>& menus) -> void {
  if (ImGui::BeginMenuBar()) {
    for (const auto& menu : menus) {
      add_menu(menu);
    }

    ImGui::EndMenuBar();
  }
}

} // namespace sbx::editor

#endif // LIBSBX_EDITOR_MENU_BAR_HPP_
