// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PARTICLES_PARTICLES_MODULE_HPP_
#define LIBSBX_PARTICLES_PARTICLES_MODULE_HPP_

#include <cstdint>
#include <vector>
#include <stdexcept>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/module.hpp>

namespace sbx::particles {

class particles_module final : public core::module<particles_module> {

  inline static const auto is_registered = register_module(stage::normal);

public:

  particles_module() {

  }

  ~particles_module() override {

  }

  auto update() -> void override {

  }

private:

}; // class particles_module

} // namespace sbx::particles

#endif // LIBSBX_PARTICLES_PARTICLES_MODULE_HPP_
