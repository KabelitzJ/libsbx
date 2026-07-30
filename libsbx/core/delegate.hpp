// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/core/delegate.hpp
 *
 * @brief A type-erased callable wrapper with small-object optimization for functors, function pointers, and member functions.
 *
 * @ingroup libsbx-core
 */

#ifndef LIBSBX_CORE_DELEGATE_HPP_
#define LIBSBX_CORE_DELEGATE_HPP_

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <libsbx/core/concepts.hpp>

#include <libsbx/memory/aligned_storage.hpp>

namespace sbx::core {

/** @brief Exception type that is thrown when a delegate that does not hold a handle is invoked. */
struct bad_delegate_call : std::runtime_error {
  bad_delegate_call()
  : std::runtime_error("bad_delegate_call") {}
}; // struct bad_delegate_call

template<typename Signature>
class delegate;

/**
 * @brief Container for functors and lambdas that makes use of small object optimization.
 *
 * @tparam Return Return type of the delegate
 * @tparam ...Args Argument types of the delegate
 */
template<typename Return, typename... Args>
class delegate<Return(Args...)> {

  using static_storage_type = memory::aligned_storage_t<3 * sizeof(std::byte*), alignof(std::byte*)>;
  using dynamic_storage_type = std::byte*;

  template<typename Callable>
  inline static constexpr auto requires_dynamic_allocation_v = !(std::is_nothrow_move_constructible_v<Callable> && sizeof(Callable) <= sizeof(static_storage_type) && alignof(Callable) <= alignof(static_storage_type));

  
  union storage {
    mutable static_storage_type static_storage;
    dynamic_storage_type dynamic_storage;
  }; // union storage

public:

  /**
   * @brief Default constructor.
   */
  delegate() noexcept
  : _vtable{nullptr} { }

  /**
   * @brief Constructs a delegate from a nullptr. 
   */
  delegate(std::nullptr_t) noexcept
  : _vtable{nullptr} { }

  /**
   * @brief Construct a delegate from a functor type.
   * 
   * @tparam Callable Type of the functor
   * 
   * @param callable Forwarded reference to a functor instance 
   */
  template<callable<Return, Args...> Callable>
  requires (!std::is_same_v<std::remove_reference_t<Callable>, delegate>)
  delegate(Callable&& callable)
  : _vtable{_create_vtable<std::remove_reference_t<Callable>>()},
    _storage{_create_storage<std::remove_reference_t<Callable>>(std::forward<Callable>(callable))} { }

  delegate(Return(*callable)(Args...))
  : delegate{[callable](Args... args){ return std::invoke(callable, std::forward<Args>(args)...); }} { }
  
  template<typename Class>
  delegate(Class& instance, Return(Class::*method)(Args...))
  : delegate{_wrap_method(&instance, method)} { }

  template<typename Class>
  delegate(Class& instance, Return(Class::*method)(Args...)const)
  : delegate{_wrap_method(&instance, method)} { }

  template<typename Class>
  delegate(const Class& instance, Return(Class::*method)(Args...)const)
  : delegate{_wrap_method(&instance, method)} { }

  delegate(const delegate& other)
  : _vtable{other._vtable} {
    if (_vtable) {
      _vtable->copy(other._storage, _storage);
    }
  }

  delegate(delegate&& other) noexcept
  : _vtable{std::exchange(other._vtable, nullptr)} {
    if (_vtable) {
      _vtable->move(other._storage, _storage);
    }
  }

  ~delegate() {
    reset();
  }

  auto operator=(const delegate& other) -> delegate& {
    if (this != &other) {
      if (this != &other) {
        auto tmp = delegate{other};
        swap(tmp);
      }
    }

    return *this;
  }

  auto operator=(delegate&& other) noexcept -> delegate& {
    if (this != &other) {
      reset();

      _vtable = std::exchange(other._vtable, nullptr);

      if (_vtable) {
        _vtable->move(other._storage, _storage);
      }
    }

    return *this;
  }

  auto operator=(std::nullptr_t) noexcept -> delegate& {
    reset();

    return *this;
  }

  auto invoke(Args&&... args) const -> Return {
    if (!_vtable) {
      throw bad_delegate_call{};
    }

    return std::invoke(_vtable->invoke, _storage, std::forward<Args>(args)...);
  }

  auto operator()(Args&&... args) const -> Return {
    return invoke(std::forward<Args>(args)...);
  }

  auto is_valid() const noexcept {
    return _vtable != nullptr;
  }

  operator bool() const noexcept {
    return is_valid();
  }

  auto operator==(std::nullptr_t) const noexcept -> bool {
    return _vtable == nullptr;
  }

  auto reset() noexcept -> void {
    if (_vtable) {
      _vtable->destroy(_storage);
      _vtable = nullptr;
    }
  }

  auto swap(delegate& other) noexcept -> void {
    using std::swap;

    swap(_vtable, other._vtable);
    swap(_storage, other._storage);
  }

  auto uses_dynamic_storage() const noexcept -> bool {
    return _vtable && _vtable->uses_dynamic_storage;
  }

private:

  template<typename Class>
  auto _wrap_method(Class* instance, Return(Class::*method)(Args...)) {
    return [instance, method](Args... args){ return std::invoke(method, instance, std::forward<Args>(args)...); };
  }

  template<typename Class>
  auto _wrap_method(Class* instance, Return(Class::*method)(Args...)const) {
    return [instance, method](Args... args){ return std::invoke(method, instance, std::forward<Args>(args)...); };
  }

  template<typename Class>
  auto _wrap_method(const Class* instance, Return(Class::*method)(Args...)const) {
    return [instance, method](Args... args){ return std::invoke(method, instance, std::forward<Args>(args)...); };
  }

  struct vtable {
    Return(*invoke)(const storage& storage, Args&&... args);
    void(*copy)(const storage& source, storage& destination);
    void(*move)(storage& source, storage& destination);
    void(*destroy)(storage& storage);

    bool uses_dynamic_storage{false};
  }; // struct vtable

  template<typename Callable>
  struct static_vtable {
    static auto invoke(const storage& storage, Args&&... args) -> Return {
      return std::invoke(reinterpret_cast<const Callable&>(storage.static_storage), std::forward<Args>(args)...);
    }

    static auto copy(const storage& source, storage& destination) -> void {
      std::construct_at(reinterpret_cast<Callable*>(&destination.static_storage), reinterpret_cast<const Callable&>(source.static_storage));
    }

    static auto move(storage& source, storage& destination) -> void {
      std::construct_at(reinterpret_cast<Callable*>(&destination.static_storage), std::move(reinterpret_cast<Callable&>(source.static_storage)));
      destroy(source);
    }

    static auto destroy(storage& storage) -> void {
      std::destroy_at(reinterpret_cast<Callable*>(&storage.static_storage));
    }
  };

  template<typename Callable>
  struct dynamic_vtable {
    static auto invoke(const storage& storage, Args&&... args) -> Return {
      return std::invoke(*reinterpret_cast<const Callable*>(storage.dynamic_storage), std::forward<Args>(args)...);
    }

    static auto copy(const storage& source, storage& destination) -> void {
      destination.dynamic_storage = reinterpret_cast<dynamic_storage_type>(new Callable{*reinterpret_cast<Callable*>(source.dynamic_storage)});
    }

    static auto move(storage& source, storage& destination) -> void {
      destination.dynamic_storage = source.dynamic_storage;
      source.dynamic_storage = nullptr;
    }

    static auto destroy(storage& storage) -> void {
      delete reinterpret_cast<Callable*>(storage.dynamic_storage);
    }
  };

  template<typename Callable>
  static auto _create_vtable() -> vtable* {
    constexpr auto requires_dynamic_allocation = requires_dynamic_allocation_v<Callable>;
    
    using vtable_type = std::conditional_t<requires_dynamic_allocation, dynamic_vtable<Callable>, static_vtable<Callable>>;

    static auto instance = vtable{
      vtable_type::invoke,
      vtable_type::copy,
      vtable_type::move,
      vtable_type::destroy,
      requires_dynamic_allocation
    };

    return &instance;
  }

  template<typename Callable>
  auto _create_storage(Callable&& callable) -> storage {
    if constexpr (requires_dynamic_allocation_v<Callable>) {
      return storage{ .dynamic_storage = reinterpret_cast<dynamic_storage_type>(new Callable{std::forward<Callable>(callable)})};
    } else {
      auto static_storage = static_storage_type{};
      std::construct_at(reinterpret_cast<Callable*>(&static_storage), std::forward<Callable>(callable));

      return storage{ .static_storage = static_storage };
    }
  }

  vtable* _vtable{};
  storage _storage{};

}; // class delegate

} // namespace sbx::core

#endif // LIBSBX_CORE_DELEGATE_HPP_
