// SPDX-License-Identifier: MIT
#include <libsbx/core/engine.hpp>

#include <range/v3/all.hpp>

#include <easy/profiler.h>

namespace sbx::core {

engine* engine::_instance{nullptr};

engine::engine(std::span<std::string_view> args)
: _cli{args},
  _is_running{false},
  _application{nullptr} {
  utility::assert_that(_instance == nullptr, "Engine instance already exists");

  _instance = this;

  if constexpr (sbx::utility::is_build_configuration_debug_v) {
    sbx::utility::logger<"core">::debug("Cli args:");

    for (const auto& arg : args) {
      sbx::utility::logger<"core">::debug("  {}", arg);
    }
  }

  for (auto&& [type, factory] : module_manager::_factories() | ranges::views::filter([](const auto& entry) { return entry.has_value(); }) | ranges::views::enumerate) {
    _create_module(type, *factory);
  }
}

engine::~engine() {
  _application.reset();

  for (auto&& [type, entry] : _modules | ranges::views::enumerate | std::views::reverse) {
    _destroy_module(type);
  }

  _instance = nullptr;
}

auto engine::delta_time() -> units::second {
  return _instance->_delta_time;
}

auto engine::fixed_delta_time() -> units::second {
  return units::second{1.0f / 60.0f};
}

auto engine::time() -> units::second {
  return _instance->_time;
}

auto engine::quit() -> void {
  _instance->_is_running = false;
}

auto engine::cli() noexcept -> core::cli& {
  return _instance->_cli;
}

auto engine::settings() noexcept -> core::settings& {
  return _instance->_settings;
}

auto engine::_run_main_loop() -> void {
  using clock_type = std::chrono::high_resolution_clock;

  _is_running = true;

  auto last = clock_type::now();

  auto fixed_accumulator = units::second{};

  while (_is_running) {
    SBX_MEMORY_SCOPE(memory::allocation_category::engine);

    const auto now = clock_type::now();
    const auto delta_time = std::chrono::duration_cast<std::chrono::duration<std::float_t>>(now - last).count();
    last = now;


    _delta_time = units::second{delta_time};
    _time += _delta_time;

    fixed_accumulator += _delta_time;

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

auto engine::_create_module(const std::uint32_t type, const module_factory& factory) -> void {
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

auto engine::_destroy_module(const std::uint32_t type) -> void {
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

auto engine::_update_stage(stage stage) -> void {
  if (auto entry = _module_by_stage.find(stage); entry != _module_by_stage.end()) {
    for (const auto& type : entry->second) {
      _modules.at(type)->update();
    }
  }
}

} // namespace sbx::core
