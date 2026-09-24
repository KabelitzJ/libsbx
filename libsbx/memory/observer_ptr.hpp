// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MEMORY_OBSERVER_PTR_HPP_
#define LIBSBX_MEMORY_OBSERVER_PTR_HPP_

#include <functional>
#include <memory>
#include <utility>

#include <libsbx/utility/assert.hpp>

namespace sbx::memory {

/** @brief A smart pointer type whose element_type is Value, e.g. std::shared_ptr<Value>. */
template<typename Type, typename Value>
concept smart_pointer = requires(Type instance) {
  typename Type::element_type;
  requires std::same_as<typename Type::element_type, Value>;
  { instance.get() } -> std::same_as<Value*>;
}; // concept smart_pointer

/**
 * @brief A non-owning pointer, distinguishing "I hold a pointer I don't own" call sites from raw
 * pointer parameters/members that might mean ownership by convention elsewhere in the codebase.
 *
 * @tparam Type The pointee type.
 *
 * @note Adds a debug-only null check (via utility::assert_that) to the dereference operators;
 * compiled out in release builds like every other utility::assert_that use.
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

  /** @brief Constructs from any smart pointer holding this same Type, observing (not sharing ownership of) its pointee. */
  template<smart_pointer<value_type> Pointer>
  constexpr observer_ptr(const Pointer& value) noexcept
  : _value{value.get()} { }

  /**
   * @brief Converting constructor from an observer_ptr<Other>, e.g. observer_ptr<Derived> to
   * observer_ptr<Base>.
   *
   * @tparam Other The source observer_ptr's pointee type; must convert implicitly to Type*.
   *
   * @param other The observer_ptr to convert from.
   */
  template<typename Other>
  requires (std::is_convertible_v<Other*, pointer>)
  constexpr observer_ptr(const observer_ptr<Other>& other) noexcept
  : _value{other.get()} { }

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

  /** @return The observed pointer, resetting this observer_ptr to null. */
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

  /** @throws assertion_failure If null (debug builds only — see utility::assert_that). */
  constexpr auto operator->() const noexcept -> const_pointer {
    utility::assert_that(is_valid(), "Cannot dereference a null pointer.");
    return _value;
  }

  /** @copydoc operator-> */
  constexpr auto operator->() noexcept -> pointer {
    utility::assert_that(is_valid(), "Cannot dereference a null pointer.");
    return _value;
  }

  /** @copydoc operator-> */
  constexpr auto operator*() const noexcept(noexcept(*std::declval<pointer>())) -> std::add_const_t<std::add_lvalue_reference_t<value_type>> {
    utility::assert_that(is_valid(), "Cannot dereference a null pointer.");
    return *_value;
  }

  /** @copydoc operator-> */
  constexpr auto operator*() noexcept(noexcept(*std::declval<pointer>())) -> std::add_lvalue_reference_t<value_type> {
    utility::assert_that(is_valid(), "Cannot dereference a null pointer.");
    return *_value;
  }

  [[nodiscard]] constexpr auto get() const noexcept -> const_pointer {
    return _value;
  }

  [[nodiscard]] constexpr auto get() noexcept -> pointer {
    return _value;
  }

private:

  pointer _value{};

}; // class observer_ptr

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

/** @return An observer_ptr observing value. */
template<typename Type>
constexpr auto make_observer(Type* value) noexcept -> observer_ptr<Type> {
  return observer_ptr<Type>{value};
}

/** @copydoc make_observer */
template<typename Type>
constexpr auto make_observer(Type& value) noexcept -> observer_ptr<Type> {
  return observer_ptr<Type>{std::addressof(value)};
}

/** @copydoc make_observer */
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
