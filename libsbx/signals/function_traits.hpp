// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_FUNCTION_TRAITS_HPP_
#define LIBSBX_SIGNAL_FUNCTION_TRAITS_HPP_

#include <utility>
#include <ranges>
#include <cstring>

#include <libsbx/memory/observer_ptr.hpp>

namespace sbx::signals {

namespace detail {

struct a { virtual ~a() = default; void f(); virtual void g(); static void h(); };
struct b { virtual ~b() = default; void f(); virtual void g(); };
struct c : a, b { void f(); void g() override; };
struct d : virtual a { void g() override; };

union function_types {
  decltype(&d::g) dm;
  decltype(&c::g) mm;
  decltype(&c::g) mvm;
  decltype(&a::f) m;
  decltype(&a::g) vm;
  decltype(&a::h) s;
  void (*f)();
  void *o;
}; // function_types

} // namespace detail

class function_ptr {

public:

  function_ptr()
  : _size{0} {
    std::ranges::fill(_data, '\n');
  }

  template<typename Type>
  void store(const Type& value) {
    const auto* ptr = reinterpret_cast<const std::uint8_t*>(std::addressof(value));

    std::memcpy(_data, ptr, sizeof(Type));
  }

  template<typename Type>
  memory::observer_ptr<const Type> as() const {
    if (sizeof(Type) != _size) {
      return nullptr;
    }

    return reinterpret_cast<const Type*>(_data);
  }

private:

  alignas(sizeof(detail::function_types)) std::uint8_t _data[sizeof(detail::function_types)];
  std::size_t _size;

}; // struct function_ptr

template<typename, typename = void>
struct has_call_operator : std::false_type { };

template<typename Type>
struct has_call_operator<Type, std::void_t<decltype(&std::remove_reference_t<Type>::operator())>> : std::true_type { };

template<typename Type>
constexpr auto has_call_operator_v = has_call_operator<Type>::value;

template<typename Type, typename = void>
struct function_traits {
  static constexpr auto is_disconnectable_v = false;
  static constexpr auto must_check_object_v = true;

  static auto ptr(const Type& /*t*/, function_ptr& /*d*/) -> void {

  }

  static auto eq(const Type& /*t*/, const function_ptr& /*d*/) -> bool {
    return false;
  }
}; // struct function_traits

template<typename Type>
struct function_traits<Type, std::enable_if_t<std::is_function_v<Type>>> {
  static constexpr bool is_disconnectable_v = true;
  static constexpr bool must_check_object_v = false;

  static auto ptr(Type& value, function_ptr& ptr) -> void {
    ptr.store(std::addressof(value));
  }

  static auto eq(Type& value, const function_ptr& ptr) -> bool {
    const auto* result = ptr.as<const Type*>();

    return result && *result == std::addressof(value);
  }
}; // struct function_traits

template<typename Type>
struct function_traits<Type*, std::enable_if_t<std::is_function_v<Type>>> {
  static constexpr bool is_disconnectable_v = true;
  static constexpr bool must_check_object_v = false;

  static auto ptr(memory::observer_ptr<Type> value, function_ptr& ptr) -> void {
    function_traits<Type>::ptr(*value, ptr);
  }
  
  static auto eq(memory::observer_ptr<Type> value, const function_ptr& ptr) -> bool {
    return function_traits<Type>::eq(*value, ptr);
  }
}; // struct function_traits

template<typename Type>
struct function_traits<Type, std::enable_if_t<std::is_member_function_pointer_v<Type>>> {
  static constexpr bool is_disconnectable_v = false;
  static constexpr bool must_check_object_v = true;
  
  static auto ptr(Type value, function_ptr& ptr) -> void {
    ptr.store(value);
  }

  static auto eq(Type value, const function_ptr& ptr) -> bool {
    const auto* result = ptr.as<const Type>();

    return result && *result == value;
  }
}; // struct function_traits

template<typename Type>
struct function_traits<Type, std::enable_if_t<has_call_operator_v<Type>>> {
  using call_type = decltype(&std::remove_reference_t<Type>::operator());

  static constexpr bool is_disconnectable_v = function_traits<call_type>::is_disconnectable_v;
  static constexpr bool must_check_object_v = function_traits<call_type>::must_check_object_v;

  static auto ptr(const Type& /*t*/, function_ptr& ptr) -> void {
    function_traits<call_type>::ptr(&Type::operator(), ptr);
  }

  static auto eq(const Type& /*t*/, const function_ptr& ptr) -> bool {
    return function_traits<call_type>::eq(&Type::operator(), ptr);
  }
}; // struct function_traits

template<typename Type>
auto get_function_ptr(const Type& value) -> function_ptr {
  auto ptr = function_ptr{};

  function_traits<std::decay_t<Type>>::ptr(value, ptr);

  return ptr;
}

template<typename Type>
auto eq_function_ptr(const Type& value, const function_ptr& ptr) -> bool {
  return function_traits<std::decay_t<Type>>::eq(value, ptr);
}

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_FUNCTION_TRAITS_HPP_
