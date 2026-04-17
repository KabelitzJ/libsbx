// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_SLOT_STATE_HPP_
#define LIBSBX_SIGNAL_SLOT_STATE_HPP_

#include <mutex>
#include <atomic>
#include <cinttypes>

#include <libsbx/signals/lockable.hpp>

namespace sbx::signals {

using group_id = std::int32_t;

class slot_state {

  template<lockable, typename...>
  friend class signal_base;

public:

  explicit slot_state(group_id group) noexcept
  : _index{0},
    _group{group},
    _is_connected{true},
    _is_blocked{false} { }

  virtual ~slot_state() = default;

  virtual auto is_connected() const noexcept -> bool {
    return _is_connected;
  }

  auto disconnect() noexcept -> bool {
    auto result = _is_connected.exchange(false);

    if (result) {
      do_disconnect();
    }

    return result;
  }

  auto is_blocked() const noexcept {
    return _is_blocked.load();
  }

  auto block() noexcept -> void {
    _is_blocked.store(true);
  }

  auto unblock() noexcept -> void {
    _is_blocked.store(false);
  }

protected:

  virtual auto do_disconnect() -> void { }

  auto index() const -> std::size_t {
    return _index;
  }

  auto set_index(std::size_t index) -> void {
    _index = index;
  }

  auto group() const -> group_id {
    return _group;
  }

private:

  std::size_t _index;
  group_id _group;
  std::atomic<bool> _is_connected;
  std::atomic<bool> _is_blocked;

}; // class slot_state

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_SLOT_STATE_HPP_
