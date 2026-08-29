// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>
#include <print>
#include <meta>

#include <libsbx/cli/cli.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/engine_config.hpp>
#include <libsbx/core/exit.hpp>
#include <libsbx/core/project.hpp>
#include <libsbx/core/command_queue.hpp>
#include <libsbx/core/threading_policy.hpp>
#include <libsbx/core/user_data_directory.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scripting/scripting_module.hpp>

#include <libsbx/render/presentation_module.hpp>
#include <libsbx/render/scene_renderer_module.hpp>

#include <libsbx/ecs/registry.hpp>

#include <libsbx/memory/memory.hpp>

#include <runtime/application.hpp>

using module_list = sbx::core::module_list<
  sbx::platform::platform_module,
  sbx::filesystem::filesystem_module,
  sbx::graphics::graphics_module,
  sbx::render::presentation_module,
  sbx::assets::assets_module,
  sbx::scenes::scenes_module,
  sbx::scripting::scripting_module,
  sbx::render::scene_renderer_module
>;

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
      .threading = sbx::core::threading_policy::multi_threaded,
      .project = sbx::core::project_config{
        .root = sbx::core::user_home_directory() / "Development" / "Game1",
        .name = "Game1"
      }
    };

    if (parsed.project) {
      const auto root = std::filesystem::path{*parsed.project};
      const auto loaded = sbx::core::project::load(root / sbx::core::project::file_name);

      config.project = sbx::core::project_config{
        .root = loaded.root(),
        .name = loaded.name()
      };
    }

    auto engine = sbx::core::basic_engine<module_list>{config};

    engine.run<runtime::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure;
  }

  return sbx::core::exit::success;
}
