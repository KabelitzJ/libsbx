// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_SLOT_HPP_
#define LIBSBX_SIGNAL_SLOT_HPP_

#include <type_traits>
#include <memory>
#include <functional>

#include <libsbx/signals/cleanable.hpp>
#include <libsbx/signals/function_traits.hpp>
#include <libsbx/signals/object_ptr.hpp>

namespace sbx::signals {

template <typename...>
class slot_base;

template <typename... Args>
using slot_ptr = std::shared_ptr<slot_base<Args...>>;

template<typename... Args>
class slot_base : public slot_state {

  template<lockable, typename...>
  friend class signal_base;

public:

  explicit slot_base(cleanable& cleaner, group_id group)
  : slot_state{group}, 
    _cleaner{cleaner} { }

  ~slot_base() override = default;

  virtual auto call_slot(Args&&...) -> void = 0;

  template<typename... Others>
  void operator()(Others&&... args) {
    if (slot_state::is_connected() && !slot_state::is_blocked()) {
      call_slot(std::forward<Others>(args)...);
    }
  }

  template<typename Callable>
  bool has_callable(const Callable& callable) const {
    auto handle = get_callable();
    return eq_function_ptr(callable, handle);
  }

  template<typename Callable>
  requires (function_traits<Callable>::must_check_object_v)
  auto has_full_callable([[maybe_unused]] const Callable& callable) const -> bool {
    return false;
  }

  template<typename Callable>
  requires (!function_traits<Callable>::must_check_object_v)
  auto has_full_callable(const Callable& callable) const -> bool {
    return has_callable(callable);
  }

  template<typename Object>
  bool has_object(const Object& object) const {
    return get_object() == get_object_ptr(object);
  }

protected:

  void do_disconnect() final {
    _cleaner.clean(this);
  }

  virtual auto get_object() const noexcept -> object_ptr {
    return nullptr;
  }

  virtual auto get_callable() const noexcept -> function_ptr {
    return get_function_ptr(nullptr);
  }

private:

  cleanable& _cleaner;

}; // class slot_base

template<typename Function, typename... Args>
class slot final : public slot_base<Args...> {

public:

  template<typename Other, typename Group>
  constexpr slot(cleanable& cleaner, Other&& other, Group group)
  : slot_base<Args...>{cleaner, group}, 
    _function{std::forward<Other>(other)} { }

protected:

  auto call_slot(Args&&... args) -> void override {
    std::invoke(_function, std::forward<Args>(args)...);
  }

  auto get_callable() const noexcept -> function_ptr override {
    return get_function_ptr(_function);
  }

private:

  std::decay_t<Function> _function;

}; // class slot

template<typename MemberFunctionPtr, typename Object, typename... Args>
class member_function_ptr_slot final : public slot_base<Args...> {
public:

  template<typename MFP, typename O>
  constexpr member_function_ptr_slot(cleanable& cleanable, MFP&& member_function_ptr, O&& object, group_id group)
  : slot_base<Args...>{cleanable, group}, 
    _member_function_ptr{std::forward<MFP>(member_function_ptr)}, 
    _object{std::forward<O>(object)} { }

protected:

  auto call_slot(Args&&... args) -> void override {
    // ((*_object).*pmf)(args...);
    std::invoke(_member_function_ptr, _object, std::forward<Args>(args)...);
  }

  auto get_callable() const noexcept -> function_ptr override {
    return get_function_ptr(_member_function_ptr);
  }

  auto get_object() const noexcept -> object_ptr override {
    return get_object_ptr(_member_function_ptr);
  }

private:

  std::decay_t<MemberFunctionPtr> _member_function_ptr;
  std::decay_t<Object> _object;

}; // member_function_ptr_slot

template<typename Function, typename WeakPtr, typename... Args>
class tracked_slot final : public slot_base<Args...> {
public:

  template<typename F, typename P>
  constexpr tracked_slot(cleanable& cleanable, F&& function, P&& object, group_id group)
  : slot_base<Args...>{cleanable, group},
    _function{std::forward<F>(function)},
    _object{std::forward<P>(object)} { }

  auto is_connected() const noexcept -> bool override {
    return !_object.expired() && slot_state::is_connected();
  }

protected:

  auto call_slot(Args&&... args) -> void override {
    auto object = _object.lock();

    if (!object) {
      slot_state::disconnect();
      return;
    }

    if (slot_state::is_connected()) {
      // func(args...);
      std::invoke(_function, std::forward<Args>(args)...);
    }
  }

  auto get_callable() const noexcept -> function_ptr override {
    return get_function_ptr(_function);
  }

  auto get_object() const noexcept -> object_ptr override {
    return get_object_ptr(_object);
  }

private:

  std::decay_t<Function> _function;
  std::decay_t<WeakPtr> _object;

}; // tracked_slot

template<typename MemberFunctionPtr, typename WeakPtr, typename... Args>
class tracked_member_function_ptr_slot final : public slot_base<Args...> {
public:

  template<typename MFP, typename O>
  constexpr tracked_member_function_ptr_slot(cleanable& cleanable, MFP&& member_function_ptr, O&& object, group_id group)
  : slot_base<Args...>{cleanable, group}, 
    _member_function_ptr{std::forward<MFP>(member_function_ptr)}, 
    _object{std::forward<O>(object)} { }

  auto is_connected() const noexcept -> bool override {
    return !_object.expired() && slot_state::is_connected();
  }

protected:

  auto call_slot(Args&&... args) -> void override {
    auto object = _object.lock();

    if (!object) {
      slot_state::disconnect();
      return;
    }

    if (slot_state::is_connected()) {
      // ((*sp).*pmf)(args...);
      std::invoke(_member_function_ptr, *_object, std::forward<Args>(args)...);
    }
  }

  auto get_callable() const noexcept -> function_ptr override {
    return get_function_ptr(_member_function_ptr);
  }

  auto get_object() const noexcept -> object_ptr override {
    return get_object_ptr(_member_function_ptr);
  }

private:

  std::decay_t<MemberFunctionPtr> _member_function_ptr;
  std::decay_t<WeakPtr> _object;

}; // tracked_member_function_ptr_slot

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_SLOT_HPP_
