// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_SCRIPTING_MODULE_HPP_
#define LIBSBX_SCRIPTING_SCRIPTING_MODULE_HPP_

#include <memory>
#include <optional>
#include <utility>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>

#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/scripting/managed/runtime.hpp>

namespace sbx::scripting {

struct scripts {
  std::vector<managed::object> instances;
}; // struct scripts

struct internal_call {
  std::string type_name;
  std::string method_name;
  void* function;
}; // struct internal_call

class scripting_module final : public core::module<scripting_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<scenes::scenes_module, filesystem::filesystem_module>{});

public:

  scripting_module();

  ~scripting_module() override;

  auto update() -> void override;

  auto load_assembly(const std::filesystem::path& assembly_path, std::initializer_list<internal_call> bindings = {}) -> void;

  auto instantiate(const scenes::node node, std::string_view class_name) -> managed::object;

private:

  static auto _exception_callback(std::string_view message) -> void {
    throw utility::runtime_error{"{}", message};
  }

  std::filesystem::path _assembly_path;

  scripting::managed::runtime _runtime;
  scripting::managed::assembly_load_context _context;
  scripting::managed::assembly _core_assembly;

}; // class scripting_module

} // namespace sbx::scripting

#endif // LIBSBX_SCRIPTING_SCRIPTING_MODULE_HPP_
