// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_COPY_ON_WRITE_HPP_
#define LIBSBX_SIGNAL_COPY_ON_WRITE_HPP_

#include <utility>
#include <atomic>

namespace sbx::signals {

template<typename Type>
class copy_on_write {

  struct payload {

    template<typename... Args>
    explicit payload(Args&&... args)
    : count{1},
      value{std::forward<Args>(args)...} { }

    ~payload() = default;

    std::atomic<std::size_t> count;
    Type value;
    
  }; // struct payload

  template<typename Other>
  friend auto swap(copy_on_write<Other>& lhs, copy_on_write<Other>& rhs) noexcept -> void;

public:

  using value_type = Type;

  copy_on_write()
  : _payload{new payload{}} { }

  template<typename Other>
  requires (!std::is_same_v<std::decay_t<Other>, copy_on_write>)
  explicit copy_on_write(Other&& other)
  : _payload{new payload{std::forward<Other>(other)}} { }

  copy_on_write(const copy_on_write& other) noexcept
  : _payload{other._payload} {
    if (_payload) {
      _payload->count.fetch_add(1, std::memory_order_relaxed);
    }
  }

  copy_on_write(copy_on_write&& other) noexcept
  : _payload{std::exchange(other._payload, nullptr)} {}

  ~copy_on_write() {
    _release();
  }

  auto operator=(const copy_on_write& other) noexcept -> copy_on_write& {
    if (this != &other) {
      _release();
      _payload = other._payload;

      if (_payload) {
        _payload->count.fetch_add(1, std::memory_order_relaxed);
      }
    }

    return *this;
  }

  auto operator=(copy_on_write&& other) noexcept -> copy_on_write& {
    if (this != &other) {
      _release();
      _payload = std::exchange(other._payload, nullptr);
    }

    return *this;
  }

  auto read() const noexcept -> const value_type& {
    return _payload->value;
  }

  auto write() -> value_type& {
    if (!_is_unique()) {
      *this = copy_on_write{read()};
    }

    return _payload->value;
  }

private:

  auto _release() -> void {
    if (_payload && _payload->count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      delete _payload;
    }

    _payload = nullptr;
  }

  auto _is_unique() const -> bool {
    return _payload->count == 1;
  }

  payload* _payload;

}; // class copy_on_write

template<typename Type>
inline auto swap(copy_on_write<Type>& lhs, copy_on_write<Type>& rhs) noexcept -> void {
  using std::swap;
  swap(lhs._payload, rhs._payload);
}

template<typename Type>
auto cow_read(const Type& value) -> const Type& {
  return value;
}

template<typename Type>
auto cow_read(copy_on_write<Type>& value) -> const Type& {
  return value.read();
}

template<typename Type>
auto cow_write(Type& value) -> Type& {
  return value;
}

template<typename Type>
auto cow_write(copy_on_write<Type>& value) -> Type& {
  return value.write();
}

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_COPY_ON_WRITE_HPP_
