#ifndef LIBSBX_SCENES_COMPONENTS_SELECTION_TAG_HPP_
#define LIBSBX_SCENES_COMPONENTS_SELECTION_TAG_HPP_

#include <libsbx/math/random.hpp>

namespace sbx::scenes {

class selection_tag final {

public:

  selection_tag()
  : _value{_next_value()} { }

  explicit selection_tag(std::uint32_t value)
  : _value{value} { }

  inline static const auto null() -> selection_tag {
    return selection_tag{0u};
  }

  auto value() const noexcept -> std::uint32_t {
    return _value;
  }

  operator std::uint32_t() const noexcept {
    return _value;
  }

private: 

  inline static auto _next_value() -> std::uint32_t {
    static auto current_value = std::uint32_t{1u};
    return current_value++;
  }

  std::uint32_t _value;

}; // class selection_tag

} // namespace sbx::scenes

template<>
struct std::hash<sbx::scenes::selection_tag> {
  auto operator()(const sbx::scenes::selection_tag& selection_tag) const noexcept -> std::size_t {
    return selection_tag.value();
  }
}; // struct std::hash<sbx::math::uuid>

#endif // LIBSBX_SCENES_COMPONENTS_SELECTION_TAG_HPP_
