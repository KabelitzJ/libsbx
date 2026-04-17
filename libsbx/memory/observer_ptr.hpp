// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MEMORY_OBSERVER_PTR_HPP_
#define LIBSBX_MEMORY_OBSERVER_PTR_HPP_

#include <memory>
#include <utility>

#include <libsbx/utility/assert.hpp>

namespace sbx::memory {

template<typename Type, typename Value>
concept smart_pointer = requires(Type t, Value v) {
  { Type::value_type} -> std::same_as<Value>;
  { t.get() } -> std::same_as<Value>;
}; // concept smart_pointer

/**
 * @brief A non-owning pointer that can be used to observe the value of a pointer.
 * 
 * @tparam Type The type of the pointer.
 * 
 * @since 1.0.0
 * 
 * @note This class adds null checks to the pointer dereference operators in debug builds. 
 */
template<typename Type>
class observer_ptr {

public:

  using value_type = Type;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  constexpr observer_ptr() noexcept = default;

  constexpr observer_ptr(std::nullptr_t) noexcept
  : _value{nullptr} { }

  constexpr observer_ptr(pointer value) noexcept
  : _value{value} { }

  template<smart_pointer<value_type> Pointer>
  constexpr observer_ptr(const Pointer& value) noexcept
  : _value{value.get()} { }

  template<typename Other>
  requires (std::is_convertible_v<Other*, pointer>)
  constexpr observer_ptr(observer_ptr<Other>& other) noexcept
  : _value{reinterpret_cast<pointer>(other.get())} { }

  constexpr observer_ptr(const observer_ptr&) noexcept = default;

  constexpr observer_ptr(observer_ptr&&) noexcept = default;

  constexpr ~observer_ptr() noexcept = default;

  constexpr auto operator=(const observer_ptr&) noexcept -> observer_ptr& = default;

  constexpr auto operator=(observer_ptr&&) noexcept -> observer_ptr& = default;

  constexpr auto operator=(std::nullptr_t) noexcept -> observer_ptr& {
    _value = nullptr;
    return *this;
  }

  constexpr auto operator=(pointer value) noexcept -> observer_ptr& {
    _value = value;
    return *this;
  }

  constexpr auto release() noexcept -> pointer {
    auto value = _value;
    _value = nullptr;
    return value;
  }

  constexpr auto reset(pointer value = nullptr) noexcept -> void {
    _value = value;
  }

  constexpr auto swap(observer_ptr& other) noexcept -> void {
    std::swap(_value, other._value);
  }

  constexpr auto is_valid() const noexcept -> bool {
    return _value != nullptr;
  }

  constexpr operator bool() const noexcept {
    return is_valid();
  }

  constexpr auto operator->() const noexcept -> const_pointer {
    utility::assert_that(is_valid(), "Cannot dereference a null pointer.");
    return _value;
  }

  constexpr auto operator->() noexcept -> pointer {
    utility::assert_that(is_valid(), "Cannot dereference a null pointer.");
    return _value;
  }

  constexpr auto operator*() const noexcept(noexcept(*std::declval<pointer>())) -> std::add_const_t<std::add_lvalue_reference_t<value_type>> {
    utility::assert_that(is_valid(), "Cannot dereference a null pointer.");
    return *_value;
  }

  constexpr auto operator*() noexcept(noexcept(*std::declval<pointer>())) -> std::add_lvalue_reference_t<value_type> {
    utility::assert_that(is_valid(), "Cannot dereference a null pointer.");
    return *_value;
  }

  constexpr auto get() const noexcept -> const_pointer {
    return _value;
  }

  constexpr auto get() noexcept -> pointer {
    return _value;
  }

private:

  pointer _value{};

}; // class observer_ptr

/**
 * @brief Compares two observer pointers for equality.
 * 
 * @tparam Type The type of the pointer.
 * 
 * @param lhs The left hand side of the comparison.
 * @param rhs The right hand side of the comparison.
 * 
 * @return true if the pointers are equal, false otherwise.
 */
template<typename Type>
constexpr auto operator==(const observer_ptr<Type>& lhs, const observer_ptr<Type>& rhs) noexcept -> bool {
  return lhs.get() == rhs.get();
}

template<typename Type>
constexpr auto operator==(const observer_ptr<Type>& lhs, std::nullptr_t) noexcept -> bool {
  return lhs.get() == nullptr;
}

template<typename Type, smart_pointer<Type> Pointer>
constexpr auto operator==(const observer_ptr<Type>& lhs, const Pointer& rhs) noexcept -> bool {
  return lhs.get() == rhs.get();
}


/**
 * @brief Creates an observer pointer from a pointer.
 * 
 * @tparam Type The type of the pointer.
 * 
 * @param value The pointer to create the observer pointer from.
 * 
 * @return An observer pointer to the pointer. 
 */
template<typename Type>
constexpr auto make_observer(Type* value) noexcept -> observer_ptr<Type> {
  return observer_ptr<Type>{value};
}

template<typename Type>
constexpr auto make_observer(Type& value) noexcept -> observer_ptr<Type> {
  return observer_ptr<Type>{std::addressof(value)};
}

template<typename Type, smart_pointer<Type> Pointer>
constexpr auto make_observer(Pointer& value) noexcept -> observer_ptr<Type> {
  return observer_ptr<Type>{value.get()};
}

} // namespace sbx::memory

template<typename Type>
struct std::hash<sbx::memory::observer_ptr<Type>> {
  constexpr auto operator()(const sbx::memory::observer_ptr<Type>& value) const noexcept -> std::size_t {
    return std::hash<std::uintptr_t>{}(reinterpret_cast<std::uintptr_t>(value.get()));
  }
};

#endif // LIBSBX_MEMORY_OBSERVER_PTR_HPP_
