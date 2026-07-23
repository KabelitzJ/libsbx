// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_SCENES_MODULE_HPP_
#define LIBSBX_SCENES_SCENES_MODULE_HPP_

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scene.hpp>

namespace sbx::scenes {

class scenes_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<assets::assets_module>;

  scenes_module();

  ~scenes_module();

  auto update() -> void;

  [[nodiscard]] auto active_scene() noexcept -> scene& {
    return _scene;
  }

  [[nodiscard]] auto active_scene() const noexcept -> const scene& {
    return _scene;
  }

private:

  scene _scene{};

}; // class scenes_module

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENES_MODULE_HPP_
