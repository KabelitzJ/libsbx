// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <span>
#include <vector>
#include <print>
#include <meta>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/engine_config.hpp>
#include <libsbx/core/exit.hpp>
#include <libsbx/core/command_queue.hpp>
#include <libsbx/core/threading_policy.hpp>

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

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  try {
    auto config = sbx::core::engine_config{
      .threading = sbx::core::threading_policy::multi_threaded,
      .project = sbx::core::project_config{
        .root = "runtime",
        .name = "Runtime"
      }
    };

    auto engine = sbx::core::basic_engine<module_list>{args, config};

    // No sbx::render::ui_module in the module_list at all — that omission alone is what makes
    // runtime UI-less, the Hazel-Runtime-equivalent ("disables ImGui") tier. presentation_module
    // still owns the swapchain and scene_renderer_module still renders/presents the scene.
    engine.run<runtime::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure;
  }

  return sbx::core::exit::success;
}
