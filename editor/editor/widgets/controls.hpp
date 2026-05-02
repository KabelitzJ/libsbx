// SPDX-License-Identifier: MIT
#ifndef EDITOR_WIDGETS_CONTROLS_HPP_
#define EDITOR_WIDGETS_CONTROLS_HPP_

#include <string>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/models/material.hpp>

#include <editor/bindings/imgui.hpp>
#include <editor/panels/texture_cache.hpp>

namespace editor {

class controls {

public:

  controls() = delete;

  static auto vector3(const std::string& label, sbx::math::vector3& values, std::float_t reset_value = 0.0f, std::float_t speed = 0.05f) -> bool;

  static auto color3(const char* label, sbx::math::color& color) -> bool;

  static auto color4(const char* label, sbx::math::color& color) -> bool;

  static auto reset_drag_float(const char* label, std::float_t& value, std::float_t reset_value, std::float_t speed = 0.05f, std::float_t min_value = 0.0f, std::float_t max_value = 0.0f, const char* format = "%.3f") -> bool;

  template<typename... Args>
  static auto labeled_text(const char* label, fmt::format_string<Args...> fmt, Args&&... args) -> void;

  static auto texture_slot(const char* label, sbx::models::texture_slot& slot, texture_cache& cache, const char* drag_drop_payload) -> void;

  template<typename Enum>
  requires (std::is_enum_v<Enum>)
  static auto enum_combo(const char* label, Enum& value) -> bool;

}; // class controls

} // namespace editor

#include <editor/widgets/controls.ipp>

#endif // EDITOR_WIDGETS_CONTROLS_HPP_
