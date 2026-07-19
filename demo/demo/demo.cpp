// SPDX-License-Identifier: MIT
#include <span>
#include <vector>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/core.hpp>

#include <libsbx/engine.hpp>

#include <demo/application.hpp>

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  try {
    auto engine = sbx::engine{args};

    engine.run<demo::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure;
  }

  return sbx::core::exit::success;
}
