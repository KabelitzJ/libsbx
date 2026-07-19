// SPDX-License-Identifier: MIT
#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/target.hpp>

namespace sbx::core {

engine* engine::_instance{nullptr};

engine::engine(std::span<std::string_view> args)
: _args{args.begin(), args.end()} {
  utility::assert_that(_instance == nullptr, "Engine instance already exists");

  _instance = this;

  if constexpr (utility::is_build_type_debug_v) {
    utility::logger<"core">::debug("Cli args:");

    for (const auto& arg : _args) {
      utility::logger<"core">::debug("  {}", arg);
    }
  }
}

engine::~engine() {
  _instance = nullptr;
}

auto engine::delta_time() -> units::seconds {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");

  return _instance->_delta_time;
}

auto engine::fixed_delta_time() -> units::seconds {
  return units::seconds{1.0f / 60.0f};
}

auto engine::time() -> units::seconds {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");

  return _instance->_time;
}

auto engine::args() -> const std::vector<std::string_view>& {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");

  return _instance->_args;
}

auto engine::quit() -> void {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");

  _instance->_is_running = false;
}

} // namespace sbx::core
