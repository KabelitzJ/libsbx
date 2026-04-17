// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_CONNECTION_HPP_
#define LIBSBX_SIGNAL_CONNECTION_HPP_

#include <memory>

#include <libsbx/signals/lockable.hpp>
#include <libsbx/signals/slot_state.hpp>

namespace sbx::signals {

class connection_blocker {

  friend class connection;

public:

  connection_blocker() = default;

  connection_blocker(const connection_blocker& other) = delete;

  connection_blocker(connection_blocker&& other) noexcept
  : _state{std::move(other._state)} { }

  ~connection_blocker() noexcept { 
    release(); 
  }

  auto operator=(const connection_blocker& other) -> connection_blocker& = delete;

  auto operator=(connection_blocker&& other) noexcept -> connection_blocker& {
    release();
    _state.swap(other._state);

    return *this;
  }

private:

  explicit connection_blocker(std::weak_ptr<slot_state> state) noexcept
  : _state{std::move(state)} {
    if (auto state = _state.lock()) {
      state->block();
    }
  }

  void release() noexcept {
    if (auto state = _state.lock()) {
      state->unblock();
    }
  }

private:

  std::weak_ptr<slot_state> _state;

}; // class connection_blocker

class connection {

  template<lockable, typename...> 
  friend class signal_base;

public:

    connection() = default;

    connection(const connection&) noexcept = default;

    connection(connection&&) noexcept = default;

    virtual ~connection() = default;

    auto operator=(const connection& other) noexcept -> connection& = default;

    auto operator=(connection&& other) noexcept -> connection& = default;

    bool is_valid() const noexcept {
      return !_state.expired();
    }

    bool is_connected() const noexcept {
      const auto state = _state.lock();
      return state && state->is_connected();
    }

    bool disconnect() noexcept {
      auto state = _state.lock();
      return state && state->disconnect();
    }

    bool is_blocked() const noexcept {
      const auto state = _state.lock();
      return state && state->is_blocked();
    }

    void block() noexcept {
      if (auto state = _state.lock()) {
        state->block();
      }
    }

    void unblock() noexcept {
      if (auto state = _state.lock()) {
        state->unblock();
      }
    }

    auto blocker() const noexcept -> connection_blocker {
      return connection_blocker{_state};
    }

protected:

  explicit connection(std::weak_ptr<slot_state> state) noexcept
  : _state{std::move(state)} { }

  std::weak_ptr<slot_state> _state;

}; // class connection

class scoped_connection final : public connection {

  template <lockable, typename...> 
  friend class signal_base;

public:
  scoped_connection() = default;

  scoped_connection(const connection& c) noexcept 
  : connection{c} { }

  scoped_connection(connection&& c) noexcept
  : connection{std::move(c)} { }

  scoped_connection(const scoped_connection& other) noexcept = delete;

  scoped_connection(scoped_connection&& other) noexcept
  : connection{std::move(other._state)} { }

  ~scoped_connection() override {
    disconnect();
  }

  auto operator=(const scoped_connection& other) noexcept -> scoped_connection& = delete;

  auto operator=(scoped_connection&& other) noexcept -> scoped_connection& {
    disconnect();
    _state.swap(other._state);
    return *this;
  }

private:

  explicit scoped_connection(std::weak_ptr<slot_state> state) noexcept
  : connection{std::move(state)} { }

}; // class scoped_connection

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_CONNECTION_HPP_
