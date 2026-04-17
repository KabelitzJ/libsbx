// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_LABEL_HPP_
#define LIBSBX_UI_LABEL_HPP_

#include <string>
#include <vector>

#include <libsbx/math/vector2.hpp>

#include <libsbx/ui/element.hpp>
#include <libsbx/ui/font.hpp>

namespace sbx::ui {

class label : public element {

public:

  label() = default;

  ~label() override = default;

  auto set_text(const std::string& value) -> void;

  [[nodiscard]] auto get_text() const -> const std::string&;

  auto set_font(const font& value) -> void;

  auto set_font_size(std::float_t value) -> void;

protected:

  auto submit(const math::vector2& screen_size) -> void override;

private:

  struct text_quad {
    math::vector2 position;
    math::vector2 size;
    math::vector2 uv_min;
    math::vector2 uv_max;
  }; // struct text_quad

  auto _ensure_layout() -> void;

  const font* _font{nullptr};
  std::string _text{};
  std::float_t _font_size{16.0f};
  std::vector<text_quad> _cached_quads;
  bool _is_dirty{true};

}; // class label

} // namespace sbx::ui

#endif // LIBSBX_UI_LABEL_HPP_
