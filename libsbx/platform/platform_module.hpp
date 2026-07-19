// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PLATFORM_PLATFORM_MODULE_HPP_
#define LIBSBX_PLATFORM_PLATFORM_MODULE_HPP_

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

namespace sbx::platform {

class platform_module : public utility::noncopyable {

public:

  platform_module();

  ~platform_module();

  auto pre_update() -> void;

}; // class platform_module

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_PLATFORM_MODULE_HPP_
