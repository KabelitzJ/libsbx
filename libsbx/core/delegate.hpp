// SPDX-License-Identifier: MIT
/* 
 * Copyright (c) 2022 Jonas Kabelitz
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * You should have received a copy of the MIT License along with this program.
 * If not, see <https://opensource.org/licenses/MIT/>.
 */

/**
 * @file libsbx/core/delegate.hpp 
 */

#ifndef LIBSBX_CORE_DELEGATE_HPP_
#define LIBSBX_CORE_DELEGATE_HPP_

/**
 * @ingroup libsbx-core
 */

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

  using static_storage_type = memory::aligned_storage_t<sizeof(std::byte*), alignof(std::byte*)>;
  using dynamic_storage_type = std::byte*;

  union storage {
    static_storage_type static_storage;
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
  requires (!std::is_same_v<std::remove_reference_t<Callable>, delegate>) // Dont allow other delegates here!
  delegate(Callable&& callable)
  : _vtable{_create_vtable<std::remove_reference_t<Callable>>()},
    _storage{_create_storage<std::remove_reference_t<Callable>>(std::forward<std::remove_reference_t<Callable>>(callable))} { }

  delegate(Return(*callable)(Args...))
  : delegate{[callable](Args... args){ std::invoke(callable, std::forward<Args>(args)...); }} { }
  
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
    if (_vtable) {
      _vtable->destroy(_storage);
    }
  }

  delegate& operator=(const delegate& other) {
    if (this != &other) {
      if (_vtable) {
        _vtable->destroy(_storage);
      }

      _vtable = other._vtable;

      if (_vtable) {
        _vtable->copy(other._storage, _storage);
      }
    }

    return *this;
  }

  delegate& operator=(delegate&& other) noexcept {
    if (this != &other) {
      if (_vtable) {
        _vtable->destroy(_storage);
      }

      _vtable = std::exchange(other._vtable, nullptr);

      if (_vtable) {
        _vtable->move(other._storage, _storage);
      }
    }

    return *this;
  }

  auto invoke(Args&&... args) const {
    if (!_vtable) {
      throw std::runtime_error{"bad_delegate_call"};
    }

    return std::invoke(_vtable->invoke, _storage, std::forward<Args>(args)...);
  }

  Return operator()(Args&&... args) const {
    return invoke(std::forward<Args>(args)...);
  }

  auto is_valid() const noexcept {
    return _vtable != nullptr;
  }

  operator bool() const noexcept {
    return is_valid();
  }

private:

  template<typename Callable>
  inline static constexpr auto requires_dynamic_allocation_v = !(std::is_nothrow_move_constructible_v<Callable> && sizeof(Callable) <= sizeof(static_storage_type) && alignof(Callable) <= alignof(static_storage_type));

  template<typename Class>
  auto _wrap_method(Class* instance, Return(Class::*method)(Args...)) {
    return [instance, method](Args... args){ std::invoke(method, instance, std::forward<Args>(args)...); };
  }

  template<typename Class>
  auto _wrap_method(Class* instance, Return(Class::*method)(Args...)const) {
    return [instance, method](Args... args){ std::invoke(method, instance, std::forward<Args>(args)...); };
  }

  template<typename Class>
  auto _wrap_method(const Class* instance, Return(Class::*method)(Args...)const) {
    return [instance, method](Args... args){ std::invoke(method, instance, std::forward<Args>(args)...); };
  }

  struct vtable {
    Return(*invoke)(const storage& storage, Args&&... args);
    void(*copy)(const storage& source, storage& destination);
    void(*move)(storage& source, storage& destination);
    void(*destroy)(storage& storage);
  };

  template<typename Callable>
  struct static_vtable {
    static Return invoke(const storage& storage, Args&&... args) {
      return std::invoke(reinterpret_cast<const Callable&>(storage.static_storage), std::forward<Args>(args)...);
    }

    static void copy(const storage& source, storage& destination) {
      std::construct_at(reinterpret_cast<Callable*>(&destination.static_storage), reinterpret_cast<const Callable&>(source.static_storage));
    }

    static void move(storage& source, storage& destination) {
      std::construct_at(reinterpret_cast<Callable*>(&destination.static_storage), std::move(reinterpret_cast<Callable&>(source.static_storage)));
      destroy(source);
    }

    static void destroy(storage& storage) {
      std::destroy_at(reinterpret_cast<Callable*>(&storage.static_storage));
    }
  };

  template<typename Callable>
  struct dynamic_vtable {
    static Return invoke(const storage& storage, Args&&... args) {
      return std::invoke(*reinterpret_cast<const Callable*>(storage.dynamic_storage), std::forward<Args>(args)...);
    }

    static void copy(const storage& source, storage& destination) {
      destination.dynamic_storage = reinterpret_cast<dynamic_storage_type>(new Callable{*reinterpret_cast<Callable*>(source.dynamic_storage)});
    }

    static void move(storage& source, storage& destination) {
      destination.dynamic_storage = source.dynamic_storage;
      source.dynamic_storage = nullptr;
    }

    static void destroy(storage& storage) {
      delete reinterpret_cast<Callable*>(storage.dynamic_storage);
    }
  };

  template<typename Callable>
  static vtable* _create_vtable() {
    using vtable_type = std::conditional_t<requires_dynamic_allocation_v<Callable>, dynamic_vtable<Callable>, static_vtable<Callable>>;

    static auto instance = vtable{
      vtable_type::invoke,
      vtable_type::copy,
      vtable_type::move,
      vtable_type::destroy
    };

    return &instance;
  }

  template<typename Callable>
  storage _create_storage(Callable&& callable) {
    if constexpr (requires_dynamic_allocation_v<Callable>) {
      // switch from new to malloc/construct_at maybe
      return storage{ .dynamic_storage = reinterpret_cast<dynamic_storage_type>(new Callable{std::forward<Callable>(callable)}) };
    } else {
      auto static_storage = static_storage_type{};
      std::construct_at(reinterpret_cast<Callable*>(&static_storage), std::forward<Callable>(callable));
      return storage{ .static_storage = static_storage };
    }
  }

  vtable* _vtable{};
  mutable storage _storage{};

}; // class delegate

} // namespace sbx::core

#endif // LIBSBX_CORE_DELEGATE_HPP_
