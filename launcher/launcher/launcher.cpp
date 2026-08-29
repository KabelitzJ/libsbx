// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <span>
#include <vector>

#include <libsbx/reflection/struct.hpp>

#include <libsbx/units/units.hpp>

#include <libsbx/cli/cli.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/engine_config.hpp>
#include <libsbx/core/exit.hpp>
#include <libsbx/core/projects_module.hpp>
#include <libsbx/core/threading_policy.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/render/presentation_module.hpp>
#include <libsbx/render/ui/ui_module.hpp>

#include <launcher/application.hpp>
#include <launcher/launcher_module.hpp>

using module_list = sbx::core::module_list<
  sbx::platform::platform_module,
  sbx::filesystem::filesystem_module,
  sbx::graphics::graphics_module,
  sbx::render::presentation_module,
  sbx::render::ui_module,
  sbx::core::projects_module,
  launcher::launcher_module
>;

struct [[=sbx::reflection::named]] test {
  [[=sbx::reflection::skip]] int a;
  [[=sbx::reflection::format(".3f")]] float b;
  [[=sbx::reflection::rename("d")]] int c;
}; // struct test

struct cli_args {
  [[=sbx::cli::help("path to the project to open")]]
  std::optional<std::filesystem::path> project;
}; // struct cli_args

auto main(int argc, const char** argv) -> int {
  using namespace sbx::units::literals;

  auto args = std::vector<std::string_view>{argv, argv + argc};

  const auto parsed = sbx::cli::parse_or_exit<cli_args>(args);

  try {
    auto config = sbx::core::engine_config{
      .threading = sbx::core::threading_policy::single_threaded,
      .window_size = sbx::math::vector2u{960u, 540u}
    };

    auto engine = sbx::core::basic_engine<module_list>{config};

    engine.run<launcher::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure;
  }

  return sbx::core::exit::success;
}
