// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CORE_ENGINE_HPP_
#define LIBSBX_CORE_ENGINE_HPP_

#include <map>
#include <vector>
#include <typeindex>
#include <memory>
#include <span>
#include <string_view>
#include <cmath>
#include <chrono>
#include <ranges>

#include <libsbx/utility/concepts.hpp>
#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/type_name.hpp>
#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/units/time.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/application.hpp>
#include <libsbx/core/cli.hpp>
#include <libsbx/core/profiler.hpp>
#include <libsbx/core/settings.hpp>

namespace sbx::core {

class engine : public utility::noncopyable {

  using stage = module_manager::stage;
  using module_base = module_manager::module_base;
  using module_factory = module_manager::module_factory;

public:

  engine(std::span<std::string_view> args);

  ~engine();

  static auto delta_time() -> units::second;

  static auto fixed_delta_time() -> units::second;

  static auto time() -> units::second;

  static auto quit() -> void;

  static auto cli() noexcept -> core::cli&;

  static auto settings() noexcept -> core::settings&;

  template<typename Module>
  requires (std::is_base_of_v<module_base, Module>)
  [[nodiscard]] static auto get_module() -> Module& {
    const auto type = type_id<Module>::value();

    auto& modules = _instance->_modules;

    if (type >= modules.size() || !modules[type]) {
      throw std::runtime_error{fmt::format("Failed to find module '{}'", utility::type_name<Module>())};
    }

    return *static_cast<Module*>(modules[type]);
  }

  template<typename Application = core::application>
  requires (std::is_same_v<core::application, Application> || std::is_base_of_v<core::application, Application>)
  [[nodiscard]] static auto get_application() -> Application& {
    utility::assert_that(_instance != nullptr, "Engine instance does not exist");
    utility::assert_that(_instance->_application != nullptr, "Engine has no application running");

    return *static_cast<Application*>(_instance->_application.get());
  }

  template<typename Application, typename... Args>
  requires (std::is_base_of_v<core::application, Application> && std::is_constructible_v<Application, Args...>)
  auto run(Args&&... args) -> void {
    utility::assert_that(_instance != nullptr, "Engine instance does not exist");
    utility::assert_that(!_is_running, "Engine instance is already running");

    _application = std::make_unique<Application>(std::forward<Args>(args)...);

    _run_main_loop();
  }

private:

  auto _run_main_loop() -> void;

  auto _create_module(const std::uint32_t type, const module_factory& factory) -> void;

  auto _destroy_module(const std::uint32_t type) -> void;

  auto _update_stage(stage stage) -> void;

  static engine* _instance;

  units::second _delta_time;
  units::second _time;

  bool _is_running{};
  // std::vector<std::string_view> _args{};
  core::cli _cli;
  // core::profiler _profiler;
  core::settings _settings;

  std::unique_ptr<application> _application;

  std::vector<module_base*> _modules{};
  std::map<stage, std::vector<std::uint32_t>> _module_by_stage{};

}; // class engine

} // namespace sbx::core

#endif // LIBSBX_CORE_ENGINE_HPP_
