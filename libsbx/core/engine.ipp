// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/core/engine.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/type_name.hpp>

namespace sbx::core {

template<module Module>
inline auto engine::get_module() -> Module& {
  auto* instance = detail::module_instance<Module>::pointer;

  utility::assert_that(instance != nullptr, fmt::format("Module '{}' is not part of the running engine's module composition", utility::type_name<Module>()));

  return *instance;
}

template<typename Application>
requires (std::is_same_v<core::application, Application> || std::is_base_of_v<core::application, Application>)
inline auto engine::get_application() -> Application& {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");
  utility::assert_that(_instance->_application != nullptr, "Engine has no application running");

  return *static_cast<Application*>(_instance->_application.get());
}

template<module... Modules>
requires (utility::are_unique_v<Modules...> && detail::dependencies_ordered_v<Modules...>)
inline basic_engine<module_list<Modules...>>::basic_engine(std::span<std::string_view> args)
: engine{args},
  _modules{} { }

template<module... Modules>
requires (utility::are_unique_v<Modules...> && detail::dependencies_ordered_v<Modules...>)
template<typename Application, typename... Args>
requires (std::is_base_of_v<core::application, Application> && std::is_constructible_v<Application, Args...>)
inline auto basic_engine<module_list<Modules...>>::run(Args&&... args) -> void {
  utility::assert_that(!_is_running, "Engine instance is already running");

  _application = std::make_unique<Application>(std::forward<Args>(args)...);

  try {
    _loop();
  } catch (...) {
    // The application must die before the modules it uses.
    _application.reset();

    throw;
  }

  _application.reset();
}

template<module... Modules>
requires (utility::are_unique_v<Modules...> && detail::dependencies_ordered_v<Modules...>)
template<stage Stage>
inline auto basic_engine<module_list<Modules...>>::_dispatch() -> void {
  _modules.for_each([](auto& module) {
    detail::invoke_stage_hook<Stage>(module);
  });
}

template<module... Modules>
requires (utility::are_unique_v<Modules...> && detail::dependencies_ordered_v<Modules...>)
inline auto basic_engine<module_list<Modules...>>::_loop() -> void {
  using clock_type = std::chrono::steady_clock;

  // Longest frame the simulation will try to catch up on. Anything above
  // this (breakpoints, window drags, hitches) is dropped instead of
  // triggering a fixed update burst.
  constexpr auto max_delta_time = 0.25f;

  _is_running = true;

  auto last = clock_type::now();

  auto fixed_accumulator = units::seconds{};

  while (_is_running) {
    const auto now = clock_type::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::duration<std::float_t>>(now - last).count();

    last = now;

    _delta_time = units::seconds{std::min(elapsed, max_delta_time)};
    _time += _delta_time;

    fixed_accumulator += _delta_time;

    {
      SBX_PROFILE_SCOPE("stage::pre");
      _dispatch<stage::pre>();
    }

    {
      SBX_PROFILE_SCOPE("stage::update");
      _dispatch<stage::update>();
      _application->update();
    }

    {
      SBX_PROFILE_SCOPE("stage::post");
      _dispatch<stage::post>();
    }

    {
      SBX_PROFILE_SCOPE("stage::fixed");

      while (fixed_accumulator >= engine::fixed_delta_time()) {
        _application->fixed_update();
        _dispatch<stage::fixed>();
        fixed_accumulator -= engine::fixed_delta_time();
      }
    }

    {
      SBX_PROFILE_SCOPE("stage::render");
      _dispatch<stage::render>();
    }

    SBX_PROFILE_FRAME_MARK();
  }
}

} // namespace sbx::core
