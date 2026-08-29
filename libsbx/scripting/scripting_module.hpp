// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
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
#include <libsbx/utility/exception.hpp>

#include <libsbx/core/module.hpp>

// #include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/scripting/managed/runtime.hpp>
#include <libsbx/scripting/script_compiler.hpp>

namespace sbx::scripting {

struct scripts {
  std::vector<managed::object> instances;
}; // struct scripts

struct internal_call {
  std::string type_name;
  std::string method_name;
  void* function;
}; // struct internal_call

struct script_runtime_error : public std::runtime_error {
  using std::runtime_error::runtime_error;
}; // struct script_runtime_error

class scripting_module final : public utility::noncopyable {
  
public:

  using dependencies = core::dependency_list<filesystem::filesystem_module, scenes::scenes_module>;

  scripting_module();

  ~scripting_module();

  auto update() -> void;

  auto load_assembly(const std::filesystem::path& assembly_path, std::initializer_list<internal_call> bindings = {}) -> void;

  auto instantiate(scenes::node& node, std::string_view class_name) -> managed::object;

  /**
   * @brief Invokes "OnDestroy" on every script instance in @p target, the mirror of
   * instantiate()'s "OnCreate" call. Scene reloads (scene_serializer::load()) clear the ECS
   * registry with no lifecycle callback of their own, which would otherwise silently drop any
   * script instances created during a session (e.g. the editor's play-mode snapshot restore on
   * Stop) without ever notifying them — call this first whenever a scene's registry is about to
   * be wiped out from under live script instances.
   */
  auto run_on_destroy(scenes::scene& target) -> void;

  /**
   * @brief (Re)compiles the project's *.cs scripts (see script_compiler) and, on success,
   * reloads the game assembly_load_context from the result — discarding whatever was previously
   * loaded there first. Safe to call whenever nothing is currently instantiate()'d from the game
   * assembly (the editor only exposes this from its Recompile Scripts menu item, gated to Edit
   * mode — see play_mode_controller); unlike compile_if_stale() alone, this always reloads even
   * if nothing was stale, since an explicit "recompile" action implies "reload" too.
   */
  auto recompile_scripts() -> void;

  /** @see script_compiler::last_compile_succeeded */
  [[nodiscard]] auto last_compile_succeeded() const noexcept -> bool {
    return _script_compiler.last_compile_succeeded();
  }

private:

  static auto _exception_callback(std::string_view message) -> void;

  [[nodiscard]] auto _dotnet_directory() const -> std::filesystem::path;

  auto _load_game_assembly() -> void;

  std::filesystem::path _assembly_path;

  scripting::managed::runtime _runtime;
  scripting::managed::assembly_load_context _context;
  scripting::managed::assembly _core_assembly;

  // Separate from _context/_core_assembly (Sbx.Core/Sbx.Managed, loaded once for the process
  // lifetime) so recompiling the project's scripts never disturbs the engine's own hosting
  // assemblies — see runtime::create_assembly_load_context's doc comment.
  scripting::managed::assembly_load_context _game_context;
  scripting::managed::assembly _game_assembly;
  bool _has_game_assembly{false};

  script_compiler _script_compiler;

}; // class scripting_module

} // namespace sbx::scripting

#endif // LIBSBX_SCRIPTING_SCRIPTING_MODULE_HPP_
