// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_LAYOUT_HPP_
#define LIBSBX_UI_LAYOUT_HPP_

#include <vector>
#include <memory>
#include <limits>

#include <libsbx/math/vector2.hpp>

namespace sbx::ui {

class element;

struct rectangle {
  std::float_t x{0.0f};
  std::float_t y{0.0f};
  std::float_t width{0.0f};
  std::float_t height{0.0f};

  [[nodiscard]] auto contains(const math::vector2& point) const -> bool;

}; // struct rectangle

struct padding {
  std::float_t top{0.0f};
  std::float_t right{0.0f};
  std::float_t bottom{0.0f};
  std::float_t left{0.0f};
}; // struct padding

struct size_hints {
  math::vector2 preferred{0.0f, 0.0f};
  math::vector2 min{0.0f, 0.0f};
  math::vector2 max{std::numeric_limits<std::float_t>::max(), std::numeric_limits<std::float_t>::max()};
  std::float_t flex{0.0f};
}; // struct size_hints

class layout {

public:

  ui::padding padding{};

  layout() = default;

  virtual ~layout() = default;

  virtual auto arrange(const rectangle& bounds, std::vector<std::unique_ptr<element>>& children) -> void = 0;

protected:

  [[nodiscard]] auto content_rectangle(const rectangle& bounds) const -> rectangle;

}; // class layout

} // namespace sbx::ui

#endif // LIBSBX_UI_LAYOUT_HPP_