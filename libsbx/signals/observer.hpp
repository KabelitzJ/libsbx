// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_OBSERVER_HPP_
#define LIBSBX_SIGNAL_OBSERVER_HPP_

#include <mutex>
#include <vector>
#include <functional>

#include <libsbx/utility/lockable.hpp>

namespace sbx::signals {

struct observer_type { };

template<typename Type>
constexpr auto is_observer_v = std::is_base_of_v<observer_type, std::remove_pointer_t<std::remove_reference_t<Type>>>;

template<utility::lockable Lockable>
class observer_base : private observer_type {

  template <utility::lockable, typename ...>
  friend class signal_base;

public:

  using lockable_type = Lockable;

  virtual ~observer_base() = default;

protected:

  void disconnect_all() {
    auto lock = std::unique_lock<lockable_type>{_mutex};

    _connections.clear();
  }

private:

  void add_connection(connection connection) {
    auto lock = std::unique_lock<lockable_type>{_mutex};

    _connections.emplace_back(std::move(connection));
  }

  lockable_type _mutex;
  std::vector<scoped_connection> _connections;

}; // class observer_base

using observer_st = observer_base<utility::null_mutex>;

using observer_mt = observer_base<std::mutex>;

using observer = observer_mt;

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_OBSERVER_HPP_
