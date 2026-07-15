// SPDX-License-Identifier: MIT
#ifndef DEMO_APPLICATION_HPP_
#define DEMO_APPLICATION_HPP_

#include <libsbx/core/application.hpp>

namespace demo {

class application : public sbx::core::application {

public:

  application();

  ~application() override = default;

  auto update() -> void override;

  auto fixed_update() -> void override;

  auto is_paused() const -> bool override;

private:

  bool _is_paused;

}; // class application

} // namespace demo

#endif // DEMO_APPLICATION_HPP_
