// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_SCENES_MODULE_HPP_
#define LIBSBX_SCENES_SCENES_MODULE_HPP_

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/units/time.hpp>

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

  /**
   * @brief Whether time-driven scene systems (script `OnUpdate`, particle `elapsed`
   * accumulation, ...) should advance this frame. Defaults to true, so runtime/launcher — which
   * never call @ref set_simulating — behave exactly as before this existed. Deliberately generic:
   * this module knows nothing about "editor" or "play mode", only whether the scene is currently
   * simulating; the editor's play/pause/stop state machine (see editor::play_mode_controller)
   * drives this through @ref set_simulating instead of living here.
   */
  [[nodiscard]] auto is_simulating() const noexcept -> bool {
    return _is_simulating;
  }

  auto set_simulating(bool value) noexcept -> void {
    _is_simulating = value;
  }

  /**
   * @brief core::engine::delta_time() while simulating, else zero. The one place every
   * time-integrating system (particle `elapsed`, future animation clips, ...) should pull its dt
   * from instead of core::engine::delta_time() directly, so they all uniformly freeze together
   * whenever simulation is paused.
   */
  [[nodiscard]] auto simulation_delta_time() const -> units::seconds {
    return _is_simulating ? core::engine::delta_time() : units::seconds{};
  }

private:

  scene _scene{};
  bool _is_simulating{true};

}; // class scenes_module

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENES_MODULE_HPP_
