// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CORE_APPLICATION_HPP_
#define LIBSBX_CORE_APPLICATION_HPP_

namespace sbx::core {

class application {

  friend class engine;

public:

  virtual ~application() = default;

  virtual auto update() -> void = 0;

  virtual auto fixed_update() -> void = 0;

}; // class application

} // namespace sbx::core

#endif // LIBSBX_CORE_APPLICATION_HPP_
