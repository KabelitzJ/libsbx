// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/engine_config.hpp>
#include <libsbx/core/exit.hpp>
#include <libsbx/core/project.hpp>
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
#include <libsbx/render/ui/ui_module.hpp>

#include <libsbx/ecs/registry.hpp>

#include <editor/application.hpp>
#include <editor/editor_module.hpp>

using module_list = sbx::core::module_list<
  sbx::platform::platform_module,
  sbx::filesystem::filesystem_module,
  sbx::graphics::graphics_module,
  sbx::render::presentation_module,
  sbx::assets::assets_module,
  sbx::scenes::scenes_module,
  sbx::scripting::scripting_module,
  sbx::render::scene_renderer_module,
  sbx::render::ui_module,
  editor::editor_module
>;

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  try {
    // editor is an app, not a project — it has no assets/.sbx of its own. Absent --project (e.g.
    // a bare debugger run), it defaults to a real external test project instead.
    auto config = sbx::core::engine_config{
      .threading = sbx::core::threading_policy::single_threaded,
      .project = sbx::core::project_config{
        .root = sbx::core::user_home_directory() / "Development" / "Game1",
        .name = "Game1"
      }
    };

    // --project <root directory>: opens the project rooted there instead of the default dev
    // project above (e.g. when spawned by launcher_module). Loaded, not open_or_create'd, so a
    // bad path fails fast rather than scaffolding an empty project next to a typo.
    for (auto index = std::size_t{0u}; index < args.size(); ++index) {
      if (args[index] == "--project" && index + 1u < args.size()) {
        const auto root = std::filesystem::path{args[index + 1u]};
        const auto loaded = sbx::core::project::load(root / sbx::core::project::file_name);

        config.project = sbx::core::project_config{
          .root = loaded.root(),
          .name = loaded.name()
        };

        break;
      }
    }

    auto engine = sbx::core::basic_engine<module_list>{args, config};

    engine.run<editor::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure;
  }

  return sbx::core::exit::success;
}
