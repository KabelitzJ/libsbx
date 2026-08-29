// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <span>
#include <vector>

#include <libsbx/units/units.hpp>

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

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  using namespace sbx::units::literals;

  auto foo = 10_ms;
  auto bar = 1_s;

  auto tmp = foo + bar;

  sbx::utility::logger<"launcher">::info("tmp: {}", tmp);

  try {
    auto config = sbx::core::engine_config{
      .threading = sbx::core::threading_policy::single_threaded,
      // Projectless — the launcher never holds a project open itself; it just picks/creates one
      // and hands off to a fresh `editor --project <root>` process.
      .project = std::nullopt,
      // A project picker, not a 3D viewport — platform_module's own default (~1920x1080) would
      // make it fill the screen for no reason; half that is a much more reasonable launcher size.
      .window_size = sbx::math::vector2u{960u, 540u}
    };

    auto engine = sbx::core::basic_engine<module_list>{args, config};

    engine.run<launcher::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure;
  }

  return sbx::core::exit::success;
}
