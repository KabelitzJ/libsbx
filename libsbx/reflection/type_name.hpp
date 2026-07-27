#ifndef LIBSBX_REFLECTION_TYPE_NAME_HPP_
#define LIBSBX_REFLECTION_TYPE_NAME_HPP_

#include <meta>
#include <string_view>

namespace sbx::reflection {

template<typename Type, bool FullyQualified = false>
consteval auto type_name() -> std::string_view {
  auto r = ^^Type;

  auto name = FullyQualified
    ? std::meta::display_string_of(r)
    : std::meta::identifier_of(std::meta::template_of(r).value_or(r));

  return std::string_view{std::define_static_string(std::string{name})};
}

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_TYPE_NAME_HPP_