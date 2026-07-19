// SPDX-License-Identifier: MIT
#include <libsbx/core/engine.hpp>

#include <libsbx/utility/profiler.hpp>

namespace sbx::core {

engine* engine::_instance{nullptr};

engine::engine(std::span<std::string_view> args)
: _is_running{false},
  _application{nullptr} {
  utility::assert_that(_instance == nullptr, "Engine instance already exists");

  _instance = this;

  if constexpr (sbx::utility::is_build_type_debug_v) {
    sbx::utility::logger<"core">::debug("Cli args:");

    for (const auto& arg : args) {
      sbx::utility::logger<"core">::debug("  {}", arg);
    }
  }

  SBX_PROFILE_SCOPE_START(s0, "engine::initialize_modules");

  for (auto&& [type, factory] : module_manager::_factories() | std::ranges::views::filter([](const auto& entry) { return entry.has_value(); }) | std::ranges::views::enumerate) {
    _create_module(type, *factory);
  }

  SBX_PROFILE_SCOPE_END(s0);
}

engine::~engine() {
  _application.reset();

  for (const auto type : _construction_order | std::views::reverse) {
    auto& factory = module_manager::_factories().at(type);
    std::invoke(factory->destroy, _modules[type]);
    _modules[type] = nullptr;
  }

  _construction_order.clear();
  _instance = nullptr;
}

auto engine::delta_time() -> units::seconds {
  return _instance->_delta_time;
}

auto engine::fixed_delta_time() -> units::seconds {
  return units::seconds{1.0f / 60.0f};
}

auto engine::time() -> units::seconds {
  return _instance->_time;
}

auto engine::quit() -> void {
  _instance->_is_running = false;
}

auto engine::_run_main_loop() -> void {
  using clock_type = std::chrono::high_resolution_clock;

  _is_running = true;

  auto last = clock_type::now();

  auto fixed_accumulator = units::seconds{};

  while (_is_running) {
    const auto now = clock_type::now();
    const auto delta_time = std::chrono::duration_cast<std::chrono::duration<std::float_t>>(now - last).count();
    last = now;

    _delta_time = units::seconds{delta_time};
    _time += _delta_time;

    fixed_accumulator += _delta_time;

    _update_stage(stage::pre);

    _application->update();

    _update_stage(stage::normal);

    _update_stage(stage::post);
  
    while (fixed_accumulator >= fixed_delta_time()) {
      _application->fixed_update();
      _update_stage(stage::fixed);
      fixed_accumulator -= fixed_delta_time();
    }

    _update_stage(stage::rendering);
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
  _construction_order.push_back(type);
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
