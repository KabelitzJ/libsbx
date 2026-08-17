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

#include <libsbx/render/render_module.hpp>

#include <libsbx/ecs/registry.hpp>

#include <libsbx/memory/memory.hpp>

#include <demo/application.hpp>

struct static_string {

  const char* data;
  std::size_t size;

  consteval static_string(std::string_view view)
  : data{std::define_static_string(view)},
    size(view.size()) { }

  constexpr auto view() const noexcept -> std::string_view {
    return std::string_view{data, size};
  }

}; // struct static_string

struct getter : static_string {
  using static_string::static_string;
}; // struct getter

struct setter : static_string {
  using static_string::static_string;
}; // struct setter

struct serializable_t { };

inline constexpr auto serializable = serializable_t{};

template<typename Type, typename Annotation>
concept annotated_with = !std::meta::annotations_of_with_type(^^Type, std::meta::remove_cv(^^Annotation)).empty();

consteval auto get_annotation(std::meta::info member, std::meta::info annotation) -> std::meta::info {
  for (auto attribute : std::meta::annotations_of(member)) {
    if (std::meta::remove_cv(std::meta::type_of(attribute)) == annotation) {
      return attribute;
    }
  }

  return {};
}

template<annotated_with<serializable_t> Type, typename Callable>
constexpr auto for_each_getter(const Type& instance, Callable&& callable) -> void {
  template for (constexpr auto member : std::define_static_array(std::meta::members_of(^^Type, std::meta::access_context::unchecked()))) {
    constexpr auto annotation = get_annotation(member, ^^getter);

    if constexpr (annotation != std::meta::info{}) {
      constexpr auto name = std::meta::extract<getter>(annotation);

      std::invoke(callable, name.view(), instance.[:member:]());
    }
  }
}

class [[=serializable]] vector3 {

public:

  vector3(float x = 0.0f) : _x{x} { }

  [[=setter{"x"}]] auto x() -> float& {
    return _x;
  }

  [[=getter{"x"}]] auto x() const -> const float& {
    return _x;
  }

private:

  float _x;

}; // struct vector3

using module_list = sbx::core::module_list<
  sbx::platform::platform_module,
  sbx::filesystem::filesystem_module,
  sbx::graphics::graphics_module,
  sbx::assets::assets_module,
  sbx::scenes::scenes_module,
  sbx::scripting::scripting_module,
  sbx::render::render_module
>;

auto main(int argc, const char** argv) -> int {
  auto args = std::vector<std::string_view>{argv, argv + argc};

  try {
    // Standalone runtime: keep the threaded render path for the CPU/GPU submission overlap.
    auto config = sbx::core::engine_config{.threading = sbx::core::threading_policy::multi_threaded};

    auto engine = sbx::core::basic_engine<module_list>{args, config};

    engine.run<demo::application>();
  } catch (const std::exception& exception) {
    sbx::utility::logger<"core">::error("{}", exception.what());

    return sbx::core::exit::failure;
  }

  auto v = vector3{4};

  for_each_getter<vector3>(v, [](auto name, auto value){
    sbx::utility::logger<"demo">::info("Property: {} = {}", name, value);
  }); 

  return sbx::core::exit::success;
}
