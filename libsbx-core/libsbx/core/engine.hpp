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

#include <range/v3/all.hpp>

#include <easy/profiler.h>

#include <libsbx/utility/concepts.hpp>
#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/type_name.hpp>
#include <libsbx/utility/timer.hpp>

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

  template<typename Callable>
  requires (std::is_invocable_r_v<std::unique_ptr<application>, Callable>)
  engine(std::span<std::string_view> args, Callable&& application_creator)
  : _cli{args},
    _application{nullptr} {
    utility::assert_that(_instance == nullptr, "Engine already exists.");

    _instance = this;

    for (auto&& [type, factory] : module_manager::_factories() | ranges::views::filter([](const auto& entry) { return entry.has_value(); }) | ranges::views::enumerate) {
      _create_module(type, *factory);
    }

    // [NOTE] KAJ 2026-01-19 : We delay application creation until after all modules are created
    _application = std::invoke(std::forward<Callable>(application_creator));
  }

  ~engine() {
    _application.reset();

    for (auto&& [type, entry] : _modules | ranges::views::enumerate | std::views::reverse) {
      _destroy_module(type);
    }

    _instance = nullptr;
  }

  static auto delta_time() -> units::second {
    return _instance->_delta_time;
  }

  static auto fixed_delta_time() -> units::second {
    return units::second{1.0f / 60.0f};
  }

  static auto time() -> units::second {
    return _instance->_time;
  }

  static auto quit() -> void {
    _instance->_is_running = false;
  }

  static auto cli() noexcept -> core::cli& {
    return _instance->_cli;
  }

  // static auto profiler() noexcept -> core::profiler& {
  //   return _instance->_profiler;
  // }

  static auto settings() noexcept -> core::settings& {
    return _instance->_settings;
  }

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

  template<typename Application>
  requires (std::is_base_of_v<core::application, Application>)
  [[nodiscard]] static auto get_application() -> Application& {
    return *static_cast<Application*>(_instance->_application.get());
  }

  auto run() -> void {
    if (_is_running) {
      return;
    }

    using clock_type = std::chrono::high_resolution_clock;

    _is_running = true;

    auto last = clock_type::now();

    auto fixed_accumulator = units::second{};

    while (_is_running) {
      const auto now = clock_type::now();
      const auto delta_time = std::chrono::duration_cast<std::chrono::duration<std::float_t>>(now - last).count();
      last = now;


      _instance->_delta_time = units::second{delta_time};
      _instance->_time += _instance->_delta_time;

      fixed_accumulator += _instance->_delta_time;

      EASY_BLOCK("stage pre");
      _update_stage(stage::pre);
      EASY_END_BLOCK;

      EASY_BLOCK("application update");
      _application->update();
      EASY_END_BLOCK;

      EASY_BLOCK("stage normal");
      _update_stage(stage::normal);
      EASY_END_BLOCK;

      EASY_BLOCK("stage post");
      _update_stage(stage::post);
      EASY_END_BLOCK;

      EASY_BLOCK("stage pre_fixed");
      _update_stage(stage::pre_fixed);
      EASY_END_BLOCK;

      while (fixed_accumulator >= fixed_delta_time()) {
        EASY_BLOCK("stage fixed");
        _application->fixed_update();
        _update_stage(stage::fixed);
        fixed_accumulator -= fixed_delta_time();
        EASY_END_BLOCK;
      }

      EASY_BLOCK("stage post_fixed");
      _update_stage(stage::post_fixed);
      EASY_END_BLOCK;

      EASY_BLOCK("stage rendering");
      _update_stage(stage::rendering);
      EASY_END_BLOCK;
    }
  }

private:

  auto _create_module(const std::uint32_t type, const module_factory& factory) -> void {
    if (type < _modules.size() && _modules[type]) {
      return;
    }

    for (const auto& dependency : factory.dependencies) {
      _create_module(dependency, *module_manager::_factories().at(dependency));
    }

    if (type >= _modules.size()) {
      _modules.resize(std::max(_modules.size(), static_cast<std::size_t>(type + 1u)));
    }

    _modules[type] = std::invoke(factory.create);
    _module_by_stage[factory.stage].push_back(type);
  }

  auto _destroy_module(const std::uint32_t type) -> void {
    if (type >= _modules.size() || !_modules.at(type)) {
      return;
    }

    auto& factory = module_manager::_factories().at(type);

    for (const auto& dependency : factory->dependencies) {
      _destroy_module(dependency);
    }

    auto* module_instance = _modules.at(type);
    std::invoke(factory->destroy, module_instance);
    _modules.at(type) = nullptr;
  }

  auto _update_stage(stage stage) -> void {
    if (auto entry = _module_by_stage.find(stage); entry != _module_by_stage.end()) {
      for (const auto& type : entry->second) {
        _modules.at(type)->update();
      }
    }
  }

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
