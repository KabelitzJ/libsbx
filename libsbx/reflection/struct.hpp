#ifndef LIBSBX_REFLECTION_STRUCT_HPP_
#define LIBSBX_REFLECTION_STRUCT_HPP_

#include <meta>
#include <functional>
#include <type_traits>

#include <libsbx/reflection/annotations.hpp>

namespace sbx::reflection {

template<typename Type>
concept reflectable_struct = std::meta::is_class_type(^^Type) && !std::meta::annotations_of_with_type(^^Type, std::meta::remove_cv(^^reflected)).empty();

template<std::meta::info Member>
struct member {

  static consteval auto reflection() -> std::meta::info {
    return Member;
  }

  static consteval auto name() -> std::string_view {
    return std::meta::identifier_of(Member);
  }

  static consteval auto type() -> std::meta::info {
    return std::meta::type_of(Member);
  }

  template <typename Annotation>
  static consteval auto has() -> bool {
    return !std::meta::annotations_of_with_type(Member, ^^Annotation).empty();
  }

  template <typename Annotation>
  static consteval auto get() -> std::optional<Annotation> {
    auto annotations = std::meta::annotations_of_with_type(Member, ^^Annotation);

    if (annotations.empty()) {
      return std::nullopt;
    }

    return std::meta::extract<Annotation>(annotations.front());
  }

}; // struct member

template<typename Type>
consteval auto members() -> std::span<const std::meta::info> {
  return std::define_static_array(std::meta::nonstatic_data_members_of(^^Type, std::meta::access_context::unchecked()));
}

template<typename Type, typename Callable>
constexpr auto for_each_member(Type&& instance, Callable&& callable) -> void {
  template for (constexpr auto m : members<std::remove_cvref_t<Type>>()) {
    std::invoke(callable, member<m>{}, instance.[:m:]);
  }
}

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_STRUCT_HPP_