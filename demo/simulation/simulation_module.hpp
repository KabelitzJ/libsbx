// SPDX-License-Identifier: MIT
#ifndef DEMO_SIMULATION_SIMULATION_MODULE_HPP_
#define DEMO_SIMULATION_SIMULATION_MODULE_HPP_

#include <cstdint>
#include <functional>
#include <vector>

#include <magic_enum/magic_enum.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

#include <demo/simulation/tick_clock.hpp>

namespace demo {

enum class tick_phase : std::uint8_t {
  transport = 0,
  production,
  construction,
  population,
  district,
  phase_count,
}; // enum class tick_phase

class simulation_module final : public sbx::core::module<simulation_module> {

  inline static const auto is_registered = register_module(stage::normal);

  static constexpr auto phase_count = magic_enum::enum_count<tick_phase>();

public:

  using tick_callback = std::function<void(const tick_time&)>;

  simulation_module() {
    _phase_callbacks.resize(phase_count);

    sbx::utility::logger<"simulation">::info("Simulation module initialized");
  }

  ~simulation_module() override = default;

  auto update() -> void override {
    EASY_BLOCK("application::update");
    SBX_PROFILE_SCOPE("application::update");

    auto delta = sbx::core::engine::delta_time();

    auto ticks = _clock.advance(delta);

    for (auto i = 0u; i < ticks; ++i) {
      _run_tick();
    }
  }

  auto clock() -> tick_clock& {
    return _clock;
  }

  auto clock() const -> const tick_clock& {
    return _clock;
  }

  auto on_tick(tick_phase phase, tick_callback callback) -> void {
    auto index = static_cast<std::size_t>(phase);

    _phase_callbacks[index].push_back(std::move(callback));
  }

private:

  auto _run_tick() -> void {
    const auto& time = _clock.time();

    for (auto phase = 0u; phase < phase_count; ++phase) {
      for (auto& callback : _phase_callbacks[phase]) {
        callback(time);
      }
    }

    if (time.hour == 0 && time.tick > 0) {
      sbx::utility::logger<"simulation">::debug("Day {}, month {}, year {}", time.day, time.month, time.year);
    }
  }

  tick_clock _clock;
  std::vector<std::vector<tick_callback>> _phase_callbacks;

}; // class simulation_module

} // namespace demo

#endif // DEMO_SIMULATION_SIMULATION_MODULE_HPP_
