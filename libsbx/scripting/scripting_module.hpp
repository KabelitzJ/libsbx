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

// #include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/physics_module.hpp>
#include <libsbx/physics/contact.hpp>

#include <libsbx/canvas/canvas_module.hpp>

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

/**
 * @brief Maps a managed field's full type name (System.Type.FullName) to the script_field_type it
 * should be tracked/edited as, or nullopt if unsupported. The single source of truth both
 * scripting_module::seed_missing_field_defaults and the editor Inspector's per-field widget
 * dispatch use, so the two can never drift apart on which C# types are supported.
 *
 * A Sbx.Core.Node field is tracked/stored as the referenced node's own scenes::id uuid, same as
 * every other field type here -- Node is a reference type Sbx.Managed can't reference at compile
 * time (Sbx.Managed sits below Sbx.Core in the dependency graph), so both directions cross as a
 * raw uuid via Sbx.Managed's INativeHandle interface instead of a real `typeof(Node)` (see
 * Sbx.Managed's Object.SetFieldValue / Marshalling.MarshalReturnValue, and Node's own
 * INativeHandle implementation).
 */
[[nodiscard]] inline auto script_field_type_of(std::string_view managed_type_full_name) -> std::optional<scenes::script_field_type> {
  if (managed_type_full_name == "System.Single") { return scenes::script_field_type::float32; }
  if (managed_type_full_name == "System.Int32") { return scenes::script_field_type::int32; }
  if (managed_type_full_name == "System.Boolean") { return scenes::script_field_type::boolean; }
  if (managed_type_full_name == "System.String") { return scenes::script_field_type::string; }
  if (managed_type_full_name == "Sbx.Core.Math.Vector3") { return scenes::script_field_type::vector3; }
  if (managed_type_full_name == "Sbx.Core.Node") { return scenes::script_field_type::node; }
  if (managed_type_full_name == "Sbx.Core.Physics.LayerMask") { return scenes::script_field_type::layer_mask; }
  if (managed_type_full_name == "Sbx.Core.Material") { return scenes::script_field_type::material; }
  if (managed_type_full_name == "Sbx.Core.Math.Color") { return scenes::script_field_type::color; }
  return std::nullopt;
}

class scripting_module final : public utility::noncopyable {
  
public:

  using dependencies = core::dependency_list<filesystem::filesystem_module, scenes::scenes_module, physics::physics_module, canvas::canvas_module>;

  scripting_module();

  ~scripting_module();

  auto update() -> void;

  auto load_assembly(const std::filesystem::path& assembly_path, std::initializer_list<internal_call> bindings = {}) -> void;

  /**
   * @brief Creates one managed instance for @p class_name on @p node — a fresh CLR allocation
   * every call, applies any persisted field overrides for this node+class (see scenes::script_entry),
   * invokes "OnCreate", and appends the instance to node's runtime scripting::scripts component.
   *
   * INVARIANT: one create_instance() call per (node, class_name) pair — never cache/reuse a
   * managed::object across nodes. Two nodes referencing the same script class must always end up
   * with two independent instances; only the resolved managed::type (not an instance) is safe to
   * cache/share. A future "avoid repeated get_type lookups" optimization is fine; memoizing
   * instances by class name is not.
   */
  auto instantiate(scenes::node& node, std::string_view class_name) -> managed::object;

  /**
   * @brief Instantiates every script in @p target's persisted scenes::script_component entries —
   * one instantiate() call per entry (see instantiate()'s invariant). Called once when a scene
   * starts playing: play_mode_controller::enter_play_mode() (editor) and
   * runtime::application::application() (standalone — the scene is simulating from frame 0 there).
   * Editor startup deliberately never calls this.
   */
  auto instantiate_scene_scripts(scenes::scene& target) -> void;

  /**
   * @brief Same 2-phase create-then-OnCreate bootstrap as instantiate_scene_scripts, scoped to just
   * @p subtree_root and its descendants — for a subtree that appears after Play has already
   * started (a prefab instantiated from a running script, via Node.Instantiate). No-op if the
   * scene isn't currently simulating (self-guarded, same defensive convention as attach_script,
   * since unlike instantiate_scene_scripts this has a reachable non-simulating caller in principle).
   * Never call instantiate_scene_scripts itself again mid-Play — it would create duplicate
   * instances for every already-running node in the whole scene, not just the new subtree.
   */
  auto instantiate_subtree_scripts(scenes::scene& target, scenes::node subtree_root) -> void;

  /**
   * @brief Attaches @p class_name to @p node's persisted scenes::script_component (creating it if
   * needed; no-ops if that exact class is already attached — at most one instance of a given class
   * per node). If the scene is already simulating, also instantiate()s this one entry immediately
   * so OnCreate fires right away instead of waiting for the next play-mode entry. The single entry
   * point the editor's "Add Script" action should go through.
   */
  auto attach_script(scenes::node& node, std::string_view class_name) -> void;

  /**
   * @brief Backfills a real default (read from a throwaway instance's post-constructor field
   * values, then discarded) into @p entry.field_overrides for every public, tracked-type field of
   * @p entry's script that doesn't already have an entry — without this, the Inspector's
   * live_instance-vs-override_slot display falls back to a hardcoded 0/false/""/(none) instead of
   * whatever the C# field initializer actually set. Cheap to call repeatedly — does nothing once
   * every field already has one, so the Inspector calls this on every draw of a script section,
   * not just attach_script (which also calls it once, right after attaching): that's what actually
   * covers a script attached before this existed, or recompiled with a field added since, not just
   * a fresh attach. No live managed::type for @p entry.class_name (e.g. no compiled game assembly
   * yet) is a silent no-op.
   */
  auto seed_missing_field_defaults(scenes::node& node, scenes::script_entry& entry) -> void;

  /**
   * @brief Removes @p class_name from @p node's persisted scenes::script_component. If the scene
   * is simulating and a live instance exists, invokes OnDestroy on it first and erases it from the
   * runtime scripting::scripts component before erasing the persisted entry.
   */
  auto detach_script(scenes::node& node, std::string_view class_name) -> void;

  [[nodiscard]] auto game_assembly() const -> const managed::assembly& {
    return _game_assembly;
  }

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

  auto _apply_field_overrides(managed::object& instance, const scenes::script_entry& entry) -> void;

  /**
   * @brief physics::physics_module::on_contact_began/on_contact_ended handler -- delivers
   * OnCollisionEnter/OnCollisionExit (or OnTriggerEnter/OnTriggerExit, per event.is_trigger) to
   * every script instance on both event.node_a and event.node_b, in both directions (each side
   * gets the *other* side as its "otherUuid" argument).
   */
  auto _dispatch_collision_event(const physics::collision_event& event, bool began) -> void;

  /** @brief One direction of _dispatch_collision_event -- invokes on self's own script instances (if any), passing other's uuid. */
  auto _invoke_collision_handler(scenes::node& self, const scenes::node& other, const physics::collision_event& event, bool began) -> void;

  /** @brief canvas::canvas_module::on_button_clicked handler -- invokes OnClick on the clicked node's own script instances (if any). */
  auto _dispatch_button_click(const scenes::node& node) -> void;

  /** @brief canvas::canvas_module::on_value_changed handler -- invokes OnValueChanged on the changed node's own script instances (if any). */
  auto _dispatch_value_changed(const scenes::node& node) -> void;

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
