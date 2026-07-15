// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PLATFORM_PLATFORM_MODULE_HPP_
#define LIBSBX_PLATFORM_PLATFORM_MODULE_HPP_

#include <libsbx/core/module.hpp>

namespace sbx::platform {

class platform_module : public core::module<platform_module> {

inline static const auto is_registered = register_module(stage::normal);

public:

  platform_module();

  ~platform_module() override;

  auto update() -> void override;

}; // class platform_module

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_PLATFORM_MODULE_HPP_
