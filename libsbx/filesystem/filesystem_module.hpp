// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_FILESYSTEM_MODULE_HPP_
#define LIBSBX_FILESYSTEM_FILESYSTEM_MODULE_HPP_

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/filesystem/alias.hpp>

namespace sbx::filesystem {
  
class filesystem_module : public core::module<filesystem_module> {

  inline static const auto is_registered = register_module(stage::pre);

public:

  filesystem_module();

  ~filesystem_module() override;

  auto update() -> void override;

}; // class assets_module

} // namespace sbx::animations

#endif // LIBSBX_FILESYSTEM_FILESYSTEM_MODULE_HPP_
