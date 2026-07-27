// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_FORMATTER_HPP_
#define LIBSBX_REFLECTION_FORMATTER_HPP_

#include <string>
#include <string_view>
#include <meta>

#include <fmt/format.h>

#include <libsbx/reflection/annotations.hpp>
#include <libsbx/reflection/property.hpp>
#include <libsbx/reflection/type_name.hpp>

namespace sbx::reflection {

template<typename Property>
consteval auto property_label() -> std::string_view {
  template for (constexpr auto annotation : std::define_static_array(std::meta::annotations_of(Property::reflection()))) {
    constexpr auto type = std::meta::remove_cv(std::meta::type_of(annotation));

    if constexpr (std::meta::has_template_arguments(type) && std::meta::template_of(type) == ^^rename) {
      using annotation_type = [: type :];

      constexpr auto extracted = std::meta::extract<annotation_type>(annotation);

      return std::string_view{std::define_static_string(extracted.view())};
    }
  }

  return Property::name();
}

template<typename Property>
consteval auto property_specifier() -> std::string_view {
  template for (constexpr auto annotation : std::define_static_array(std::meta::annotations_of(Property::reflection()))) {
    constexpr auto type = std::meta::remove_cv(std::meta::type_of(annotation));

    if constexpr (std::meta::has_template_arguments(type) && std::meta::template_of(type) == ^^format) {
      using annotation_type = [: type :];

      constexpr auto extracted = std::meta::extract<annotation_type>(annotation);

      return std::string_view{std::define_static_string(extracted.view())};
    }
  }

  return std::string_view{};
}

} // namespace sbx::reflection

template<sbx::reflection::reflectable Type>
struct fmt::formatter<Type> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const Type& value, FormatContext& ctx) const -> decltype(ctx.out()) {
    auto out = fmt::format_to(ctx.out(), "{}{{", sbx::reflection::type_name<Type>());

    auto first = true;

    sbx::reflection::for_each_property(value, [&](auto property, const auto& object) {
      using property_type = decltype(property);

      if constexpr (!property_type::template has<sbx::reflection::skip>()) {
        if (!first) {
          out = fmt::format_to(out, ", ");
        }

        first = false;

        constexpr auto label = sbx::reflection::property_label<property_type>();
        constexpr auto specifier = sbx::reflection::property_specifier<property_type>();

        out = fmt::format_to(out, "{}=", label);

        if constexpr (specifier.empty()) {
          out = fmt::format_to(out, "{}", property_type::get(object));
        } else {
          out = fmt::format_to(out, fmt::runtime("{" + std::string{specifier} + "}"), property_type::get(object));
        }
      }
    });

    return fmt::format_to(out, "}}");
  }

}; // struct fmt::formatter

#endif // LIBSBX_REFLECTION_FORMATTER_HPP_
