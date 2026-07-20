// SPDX-License-Identifier: MIT
#include <span>
#include <vector>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/exit.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/ecs/registry.hpp>

#include <demo/application.hpp>

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  try {
    auto engine = sbx::core::basic_engine<sbx::platform::platform_module, sbx::graphics::graphics_module>{args};

    engine.run<demo::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure;
  }

  return sbx::core::exit::success;
}
