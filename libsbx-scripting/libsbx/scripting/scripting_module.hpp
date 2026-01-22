// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_SCRIPTING_MODULE_HPP_
#define LIBSBX_SCRIPTING_SCRIPTING_MODULE_HPP_

#include <memory>
#include <optional>
#include <utility>
#include <filesystem>
#include <unordered_map>

#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scripting/managed/runtime.hpp>

namespace sbx::scripting {

struct scripts {
  std::vector<managed::object> instances;
}; // struct scripts

class scripting_module final : public core::module<scripting_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<scenes::scenes_module>{});

public:

  scripting_module();

  ~scripting_module() override;

  auto update() -> void override;

  auto instantiate(const scenes::node node, const std::filesystem::path& assembly_path, std::string_view class_name) -> managed::object;

private:

  static auto _exception_callback(std::string_view message) -> void {
    utility::logger<"scripting">::error("{}", message);
  }

  scripting::managed::runtime _runtime;
  scripting::managed::assembly_load_context _context;
  scripting::managed::assembly _core_assembly;

}; // class scene_modules

} // namespace sbx::scripting

#endif // LIBSBX_SCRIPTING_SCRIPTING_MODULE_HPP_
