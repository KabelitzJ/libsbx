// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

#include <libsbx/cli/cli.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine_config.hpp>
#include <libsbx/core/engine.hpp>
#include <libsbx/core/exit.hpp>
#include <libsbx/core/project.hpp>
#include <libsbx/core/projects_module.hpp>
#include <libsbx/core/threading_policy.hpp>
#include <libsbx/core/user_data_directory.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/physics_module.hpp>

#include <libsbx/scripting/scripting_module.hpp>

#include <libsbx/render/presentation_module.hpp>
#include <libsbx/render/scene_renderer_module.hpp>
#include <libsbx/render/ui/ui_module.hpp>

#include <libsbx/ecs/registry.hpp>

#include <editor/application.hpp>
#include <editor/editor_module.hpp>

using module_list = sbx::core::module_list<
  sbx::core::projects_module,
  sbx::platform::platform_module,
  sbx::filesystem::filesystem_module,
  sbx::graphics::graphics_module,
  sbx::render::presentation_module,
  sbx::assets::assets_module,
  sbx::scenes::scenes_module,
  sbx::physics::physics_module,
  sbx::scripting::scripting_module,
  sbx::render::scene_renderer_module,
  sbx::render::ui_module,
  editor::editor_module
>;

struct [[=sbx::cli::args]] cli_args {
  [[=sbx::cli::required]]
  [[=sbx::cli::help("path to the project to open")]]
  std::filesystem::path project;
}; // struct cli_args

auto main(int argc, const char** argv) -> int {
  using namespace sbx::units::literals;

  auto args = std::vector<std::string_view>{argv, argv + argc};

  const auto parsed = sbx::cli::parse_or_exit<cli_args>(args);

  try {
    const auto project_file = std::filesystem::path{parsed.project};
    const auto loaded = sbx::core::project::load(project_file);

    auto config = sbx::core::engine_config{
      .threading = sbx::core::threading_policy::single_threaded,
      .project = sbx::core::project_config{
        .root = project_file.parent_path(),
        .name = loaded.name()
      }
    };

    auto engine = sbx::core::basic_engine<module_list>{config};

    engine.run<editor::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure;
  }

  return sbx::core::exit::success;
}
