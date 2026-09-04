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

  auto late_update() -> void;

  [[nodiscard]] auto active_scene() noexcept -> scene& {
    return _scene;
  }

  [[nodiscard]] auto active_scene() const noexcept -> const scene& {
    return _scene;
  }

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

  /**
   * @brief Accumulated simulation_delta_time() — i.e. core::engine::time() with pauses subtracted
   * out. Unlike core::engine::time(), this stops advancing whenever simulation is paused, so
   * anything deriving absolute-time animation (shader noise/scroll, procedural motion, ...) freezes
   * in sync with simulation_delta_time() instead of continuing to animate while paused. Updated once
   * per frame in late_update(), before render reads it.
   */
  [[nodiscard]] auto simulation_time() const -> units::seconds {
    return _simulation_time;
  }

private:

  scene _scene{};
  bool _is_simulating{true};
  units::seconds _simulation_time{};

}; // class scenes_module

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENES_MODULE_HPP_
