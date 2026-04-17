// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_CLEANABLE_HPP_
#define LIBSBX_SIGNAL_CLEANABLE_HPP_

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/signals/slot_state.hpp>

namespace sbx::signals {

struct cleanable {
  virtual ~cleanable() = default;
  virtual auto clean(memory::observer_ptr<slot_state> state) -> void = 0;
}; // struct cleanable

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_CLEANABLE_HPP_
