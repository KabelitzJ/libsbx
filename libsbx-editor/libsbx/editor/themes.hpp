// SPDX-License-Identifier: MIT
#ifndef LIBSBX_EDITOR_THEMES_HPP_
#define LIBSBX_EDITOR_THEMES_HPP_

#include <unordered_map>
#include <functional>
#include <string>

namespace sbx::editor {

class themes {

public:

  themes();

  auto apply_theme(const std::string& theme) -> void;
  auto add_theme(const std::string& name, const std::function<void()>& callback) -> void;
  auto get_themes() const -> std::vector<std::string>;
  auto get_active_theme() const -> const std::string&;

private:

  static auto set_dark_theme_colors() -> void;
  static auto set_catpuccin_mocha_colors() -> void;
  static auto set_fluent_ui_colors() -> void;
  static auto set_bess_dark_colors() -> void;
  static auto set_modern_dark_colors() -> void;
  static auto set_moonlight_colors() -> void;
  static auto set_blueish_colors() -> void;
  static auto set_foo_colors() -> void;
  static auto set_bar_colors() -> void;
  static auto set_nvpro() -> void;

  static auto apply_color_correction() -> void;

  std::unordered_map<std::string, std::function<void()>> _themes;
  std::string _active_theme;

}; // class themes

} // namespace sbx::editor

#endif // LIBSBX_EDITOR_THEMES_HPP_
