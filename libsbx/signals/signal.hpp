// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_SIGNAL_HPP_
#define LIBSBX_SIGNAL_SIGNAL_HPP_

#include <atomic>
#include <type_traits>

#include <libsbx/utility/lockable.hpp>

#include <libsbx/signals/copy_on_write.hpp>
#include <libsbx/signals/slot.hpp>
#include <libsbx/signals/connection.hpp>
#include <libsbx/signals/observer.hpp>

namespace sbx::signals {

template<utility::lockable Lockable, typename... Args>
class signal_base final : public cleanable {

  template<typename L>
  inline constexpr static auto is_thread_safe_v = !std::is_same_v<L, utility::null_mutex>;

  template<typename U, typename L>
  using cow_type = std::conditional_t<is_thread_safe_v<L>, copy_on_write<U>, U>;

  template<typename U, typename L>
  using cow_copy_type = std::conditional_t<is_thread_safe_v<L>, copy_on_write<U>, const U&>;

  using lock_type = std::unique_lock<Lockable>;
  using slot_base = signals::slot_base<Args...>;
  using slot_ptr = signals::slot_ptr<Args...>;
  using slot_list_type = std::vector<slot_ptr>;

  struct group_type { 
    slot_list_type slots; 
    group_id id; 
  }; // struct group_type

  using list_type = std::vector<group_type>;

public:

  using lockable_type = Lockable;

  signal_base() noexcept
  : _is_blocked{false} {}

  ~signal_base() override {
    disconnect_all();
  }

  signal_base(const signal_base& other) = delete;

  signal_base(signal_base&& other)
  : _is_blocked{other._is_blocked.load()} {
    auto lock = lock_type{_mutex};

    using std::swap;
    swap(_slots, other._slots);
  }

  auto operator=(const signal_base& other) -> signal_base& = delete;

  auto operator=(signal_base&& other) -> signal_base& {
    auto lock1 = lock_type{_mutex, std::defer_lock};
    auto lock2 = lock_type{other._mutex, std::defer_lock};

    std::lock(lock1, lock2);

    using std::swap;
    swap(_slots, other._slots);
    _is_blocked.store(other._is_blocked.exchange(_is_blocked.load()));

    return *this;
  }

  template<typename... As>
  auto emit(As&&... args) -> void {
    if (_is_blocked) {
      return;
    }

    auto reference = _slots_reference();

    for (const auto& group : cow_read(reference)) {
      for (const auto& slot : group.slots) {
        std::invoke(*slot, std::forward<As>(args)...);
      }
    }
  }

  template<typename... As>
  auto operator()(As&& ...args) -> void {
    emit(std::forward<As>(args)...);
  }

  template<typename... As>
  auto operator+=(As&&... args) -> connection {
    return connect(std::forward<As>(args)...);
  }

  template<std::invocable<Args...> Callable>
  auto connect(Callable&& callable, group_id id = 0) -> connection {
    using slot_type = slot<Callable, Args...>;

    auto slot = _make_slot<slot_type>(std::forward<Callable>(callable), id);

    auto conn = connection{slot};

    _add_slot(std::move(slot));

    return conn;
  }

  template<typename MemberFnPtr, typename Object>
  requires (std::is_invocable_v<MemberFnPtr, Object, Args...> && !is_observer_v<Object> && !is_weak_ptr_compatible_v<Object>)
  auto connect(MemberFnPtr&& member_fn_ptr, Object&& object, group_id id = 0) -> connection {
    using slot_type = member_function_ptr_slot<MemberFnPtr, Object, Args...>;

    auto slot = _make_slot<slot_type>(std::forward<MemberFnPtr>(member_fn_ptr), std::forward<Object>(object), id);

    auto conn = connection{slot};

    _add_slot(std::move(slot));

    return conn;
  }

  template<std::invocable<Args...> Callable, typename Trackable>
  requires (is_weak_ptr_compatible_v<Trackable>)
  auto connect(Callable&& callable, Trackable&& trackable, group_id id = 0) -> connection {
    using signals::to_weak;

    auto weak = to_weak(std::forward<Trackable>(trackable));

    using slot_type = tracked_slot<Callable, decltype(weak), Args...>;

    auto slot = _make_slot<slot_type>(std::forward<Callable>(callable), weak, id);

    auto conn = connection{slot};

    _add_slot(std::move(slot));

    return conn;
  }

  template<typename... As>
  auto connect_scoped(As&&... args) -> scoped_connection {
    return connect(std::forward<As>(args)...);
  }

  template<typename Callable>
  requires (std::is_invocable_v<Callable, Args...> || (std::is_member_function_pointer_v<Callable> && function_traits<Callable>::is_disconnectable_v))
  auto disconnect(const Callable& callable) -> std::size_t {
    return disconnect_if ([&](const auto& slot) {
      return slot->has_full_callable(callable);
    });
  }

  template<typename Object>
  requires (!std::is_invocable_v<Object, Args...> && !std::is_member_function_pointer_v<Object>)
  auto disconnect(const Object& object) -> std::size_t {
    return disconnect_if ([&](const auto& slot) {
      return slot->has_object(object);
    });
  }

  template<typename Callable, typename Object>
  auto disconnect(const Callable& callable, const Object& object) -> std::size_t {
    return disconnect_if ([&] (const auto& slot) {
      return slot->has_object(object) && slot->has_callable(callable);
    });
  }

  auto disconnect_all() -> void {
    auto lock = lock_type{_mutex};

    _clear();
  }

  void block() noexcept {
    _is_blocked.store(true);
  }

  void unblock() noexcept {
    _is_blocked.store(false);
  }

  bool blocked() const noexcept {
    return _is_blocked.load();
  }

protected:

  auto clean(memory::observer_ptr<slot_state> state) -> void override {
    auto lock = lock_type{_mutex};

    const auto index = state->index();
    const auto group_id = state->group();

    for (auto& group : cow_write(_slots)) {
      if (group.id == group_id) {
        auto& slots = group.slots;

        if (index < slots.size() && slots[index] && slots[index].get() == state.get()) {
          std::swap(slots[index], slots.back());
          slots[index]->set_index(index);
          slots.pop_back();
        }

        return;
      }
    }
  }

  template<typename Condition>
  requires (std::is_invocable_r_v<bool, Condition, const slot_ptr&>)
  auto disconnect_if (Condition&& condition) -> std::size_t {
    auto lock = lock_type{_mutex};

    auto& groups = cow_write(_slots);

    auto count = std::size_t{0};

    for (auto& group : groups) {
      auto& slots = group.slots;
      auto i = std::size_t{0};

      while (i < slots.size()) {
        if (std::invoke(condition, slots[i])) {
          using std::swap;

          swap(slots[i], slots.back());

          slots[i]->set_index(i);
          slots.pop_back();

          ++count;
        } else {
          ++i;
        }
      }
    }

    return count;
  }

private:

  auto _slots_reference() -> cow_copy_type<list_type, lockable_type> {
    auto lock = lock_type{_mutex};

    return _slots;
  }

  template<typename Slot, typename... As>
  auto _make_slot(As&& ...args) -> std::shared_ptr<Slot> {
    return std::make_shared<Slot>(*this, std::forward<As>(args)...);
  }

  auto _add_slot(slot_ptr&& slot) -> void {
    const auto group_id = slot->group();

    auto lock = lock_type{_mutex};

    auto& groups = cow_write(_slots);


    auto entry = groups.begin();

    while (entry != groups.end() && entry->id < group_id) {
      entry++;
    }


    if (entry == groups.end() || entry->id != group_id) {
      entry = groups.insert(entry, group_type{slot_list_type{}, group_id});
    }

    // add the slot
    slot->set_index(entry->slots.size());
    entry->slots.push_back(std::move(slot));
  }

  auto _clear() -> void {
    cow_write(_slots).clear();
  }

  lockable_type _mutex;
  cow_type<list_type, Lockable> _slots;
  std::atomic<bool> _is_blocked;

}; // class signal_base

template<typename... Args>
using signal_st = signal_base<utility::null_mutex, Args...>;

template<typename... Args>
using signal_mt = signal_base<std::mutex, Args...>;

template<typename... Args>
using signal = signal_mt<Args...>;

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_SIGNAL_HPP_
