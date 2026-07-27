#ifndef LIBSBX_REFLECTION_TYPE_NAME_HPP_
#define LIBSBX_REFLECTION_TYPE_NAME_HPP_

#include <meta>
#include <string_view>

namespace sbx::reflection {

template<typename Type, bool FullyQualified = false>
consteval auto type_name() -> std::string_view {
  if constexpr (!FullyQualified) {
    return std::meta::identifier_of(^^Type);
  } else {
    auto parts = std::vector<std::string_view>{};

    parts.push_back(std::meta::identifier_of(^^Type));

    auto scope = std::meta::parent_of(^^Type);

    while (std::meta::is_namespace(scope) && std::meta::has_identifier(scope)) {
      parts.push_back(std::meta::identifier_of(scope));
      scope = std::meta::parent_of(scope);
    }

    auto result = std::string{};

    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
      if (!result.empty()) {
        result += "::";
      }

      result += *it;
    }

    return std::string_view{std::define_static_string(result)};
  }
}

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_TYPE_NAME_HPP_