// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCRIPTING_SCRIPTING_MODULE_HPP_
#define LIBSBX_SCRIPTING_SCRIPTING_MODULE_HPP_

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>

#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/exception.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/physics_module.hpp>
#include <libsbx/physics/contact.hpp>

#include <libsbx/canvas/canvas_module.hpp>

#include <libsbx/render/presentation_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/scripting/managed/runtime.hpp>
#include <libsbx/scripting/script_compiler.hpp>
#include <libsbx/scripting/script_resources.hpp>

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

/**
 * @brief Maps a managed field's full type name to the script_field_type it is edited as.
 *
 * Shared by seed_missing_field_defaults and the Inspector so both agree on which C# types are supported.
 * Sbx.Core.Node fields cross as the referenced node's uuid via INativeHandle, since Sbx.Managed can't reference Sbx.Core.
 *
 * @param managed_type The field's managed type.
 *
 * @return The matching script_field_type, or nullopt if the type isn't supported.
 */
[[nodiscard]] inline auto script_field_type_of(const managed::type& managed_type) -> std::optional<scenes::script_field_type> {
  const auto full_name = std::string{managed_type.get_full_name()};
  const auto managed_type_full_name = std::string_view{full_name};

  if (managed_type_full_name == "System.Single") { return scenes::script_field_type::float32; }
  if (managed_type_full_name == "System.Int32") { return scenes::script_field_type::int32; }
  if (managed_type_full_name == "System.Boolean") { return scenes::script_field_type::boolean; }
  if (managed_type_full_name == "System.String") { return scenes::script_field_type::string; }
  if (managed_type_full_name == "Sbx.Core.Math.Vector2") { return scenes::script_field_type::vector2; }
  if (managed_type_full_name == "Sbx.Core.Math.Vector3") { return scenes::script_field_type::vector3; }
  if (managed_type_full_name == "Sbx.Core.Node") { return scenes::script_field_type::node; }
  if (managed_type_full_name == "Sbx.Core.Physics.LayerMask") { return scenes::script_field_type::layer_mask; }
  if (managed_type_full_name == "Sbx.Core.Material") { return scenes::script_field_type::material; }
  if (managed_type_full_name == "Sbx.Core.Math.Color") { return scenes::script_field_type::color; }
  if (managed_type_full_name == "Sbx.Core.Texture2D") { return scenes::script_field_type::texture; }
  if (!managed_type.get_enum_entries().empty()) { return scenes::script_field_type::enumeration; }
  return std::nullopt;
}

class scripting_module final : public utility::noncopyable {
  
public:

  using dependencies = core::dependency_list<filesystem::filesystem_module, scenes::scenes_module, physics::physics_module, canvas::canvas_module, render::presentation_module>;

  scripting_module();

  ~scripting_module();

  auto update() -> void;

  auto load_assembly(const std::filesystem::path& assembly_path, std::initializer_list<internal_call> bindings = {}) -> void;

  /**
   * @brief Creates a managed instance of @p class_name on @p node, applies its persisted field overrides, invokes OnCreate and appends it to the node's scripting::scripts.
   *
   * @warning Never reuse an instance across nodes: two nodes with the same class need two instances. Caching the resolved managed::type is fine.
   *
   * @param node The node the script belongs to.
   * @param class_name The script's full class name.
   *
   * @return The new instance.
   */
  auto instantiate(scenes::node& node, std::string_view class_name) -> managed::object;

  /**
   * @brief Instantiates every persisted script in @p target. Called once when a scene starts playing.
   *
   * @param target The scene to instantiate scripts in.
   */
  auto instantiate_scene_scripts(scenes::scene& target) -> void;

  /**
   * @brief Instantiates the scripts of a subtree that appeared after play started, such as a prefab instantiated from a script. No-op unless simulating.
   *
   * Never call instantiate_scene_scripts mid-play instead: it would duplicate every running instance.
   *
   * @param target The scene containing the subtree.
   * @param subtree_root The subtree's root node.
   */
  auto instantiate_subtree_scripts(scenes::scene& target, scenes::node subtree_root) -> void;

  /**
   * @brief Attaches @p class_name to @p node (at most one per class), instantiating it immediately if the scene is simulating.
   *
   * @param node The node to attach to.
   * @param class_name The script's full class name.
   */
  auto attach_script(scenes::node& node, std::string_view class_name) -> void;

  /**
   * @brief Fills @p entry's missing field overrides with the defaults of a throwaway instance, so the Inspector shows the C# initializers.
   *
   * Cheap once every field has an entry, so the Inspector calls it on every draw; a no-op without a compiled game assembly.
   *
   * @param node The node the script is attached to.
   * @param entry The script entry to fill.
   */
  auto seed_missing_field_defaults(scenes::node& node, scenes::script_entry& entry) -> void;

  /**
   * @brief Removes @p class_name from @p node, invoking OnDestroy on its live instance first if the scene is simulating.
   *
   * @param node The node to detach from.
   * @param class_name The script's full class name.
   */
  auto detach_script(scenes::node& node, std::string_view class_name) -> void;

  [[nodiscard]] auto game_assembly() const -> const managed::assembly& {
    return _game_assembly;
  }

  /**
   * @brief Invokes OnDestroy on every script instance in @p target and clears resources().
   *
   * Call before a scene's registry is wiped (e.g. restoring the play-mode snapshot), since scene_serializer::load() has no lifecycle callbacks.
   *
   * @param target The scene whose instances are destroyed.
   */
  auto run_on_destroy(scenes::scene& target) -> void;

  /** @brief Recompiles the project's scripts and, on success, reloads the game assembly. Only call while no game script is instantiated. */
  auto recompile_scripts() -> void;

  /**
   * @brief Whether the last script compile left a usable assembly.
   *
   * @return True if the game assembly is usable.
   */
  [[nodiscard]] auto last_compile_succeeded() const noexcept -> bool {
    return _script_compiler.last_compile_succeeded();
  }

  /**
   * @brief GPU resources and deferred geometry created by scripts; cleared by run_on_destroy and on script reload.
   *
   * @return The script resources.
   */
  [[nodiscard]] auto resources() noexcept -> script_resources& {
    return _resources;
  }

private:

  static auto _exception_callback(std::string_view message) -> void;

  [[nodiscard]] auto _dotnet_directory() const -> std::filesystem::path;

  auto _load_game_assembly() -> void;

  auto _register_managed_components() -> void;

  auto _apply_field_overrides(managed::object& instance, const scenes::script_entry& entry) -> void;

  /** @brief Delivers OnCollisionEnter/Exit (or OnTriggerEnter/Exit) to the scripts on both nodes of a contact, each receiving the other node. */
  auto _dispatch_collision_event(const physics::collision_event& event, bool began) -> void;

  auto _invoke_collision_handler(scenes::node& self, const scenes::node& other, const physics::collision_event& event, bool began) -> void;

  auto _dispatch_button_click(const scenes::node& node) -> void;

  auto _dispatch_value_changed(const scenes::node& node) -> void;

  std::filesystem::path _assembly_path;

  scripting::managed::runtime _runtime;
  scripting::managed::assembly_load_context _context;
  scripting::managed::assembly _core_assembly;

  // Separate from _context so recompiling scripts never disturbs the engine's own hosting assemblies.
  scripting::managed::assembly_load_context _game_context;
  scripting::managed::assembly _game_assembly;
  bool _has_game_assembly{false};

  script_compiler _script_compiler;

  script_resources _resources;

}; // class scripting_module

} // namespace sbx::scripting

#endif // LIBSBX_SCRIPTING_SCRIPTING_MODULE_HPP_
