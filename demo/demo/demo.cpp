// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <span>
#include <vector>
#include <print>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/exit.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scripting/scripting_module.hpp>

#include <libsbx/render/render_module.hpp>

#include <libsbx/ecs/registry.hpp>

#include <libsbx/reflection/enum.hpp>
#include <libsbx/reflection/struct.hpp>
#include <libsbx/reflection/formatter.hpp>

#include <demo/application.hpp>

using module_list = sbx::core::module_list<
  sbx::platform::platform_module,
  sbx::filesystem::filesystem_module,
  sbx::graphics::graphics_module,
  sbx::assets::assets_module,
  sbx::scenes::scenes_module,
  sbx::scripting::scripting_module,
  sbx::render::render_module
>;

static_assert(
    !std::meta::annotations_of_with_type(
        std::meta::template_of(^^sbx::math::basic_vector2<unsigned int>),
        ^^sbx::reflection::reflected).empty(),
    "reflected not seen on template");

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  // try {
  //   auto engine = sbx::core::basic_engine<module_list>{args};

  //   engine.run<demo::application>();
  // } catch (const std::exception& exception) {
  //   sbx::utility::logger<"core">::error("{}", exception.what());

  //   return sbx::core::exit::failure;
  // }

  std::println("{}", fmt::format("{}", sbx::math::vector2u{32, 1}));

  return sbx::core::exit::success;
}
