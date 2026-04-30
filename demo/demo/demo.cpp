// SPDX-License-Identifier: MIT
#include <span>
#include <ranges>
#include <stacktrace>

#include <range/v3/all.hpp>

#include <libsbx/utility/target.hpp>

#include <libsbx/core/core.hpp>
#include <libsbx/core/engine.hpp>
#include <libsbx/core/exit.hpp>

#include <demo/application.hpp>

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  try {
    auto engine = std::make_unique<sbx::core::engine>(args);

    engine->run<demo::application>();
  } catch(const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure; 
  }

  return sbx::core::exit::success;
}
