// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_SCENES_MODULE_HPP_
#define LIBSBX_SCENES_SCENES_MODULE_HPP_

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/signals/signal.hpp>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/units/time.hpp>

#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scene.hpp>

namespace sbx::scenes {

enum class [[=reflection::named]] load_scene_mode : std::uint8_t {
  single,  // unloads every loaded scene (except the persistent one) first
  additive // loads alongside the loaded scenes
}; // enum class load_scene_mode

struct loaded_scene {
  std::uint32_t handle{0u};
  std::filesystem::path path{}; // project-relative; empty for a scene never saved
  std::string name{};
  std::int32_t build_index{-1}; // position in the project's scene list; -1 when not in it
}; // struct loaded_scene

inline constexpr auto invalid_scene_handle = std::uint32_t{0xFFFFFFFFu};

/** @brief A queued load's operation id (for is_operation_done) and the handle the scene will have once loaded. */
struct queued_load {
  std::uint32_t operation{0u};
  std::uint32_t handle{invalid_scene_handle};
}; // struct queued_load

struct active_scene_changed_event {
  std::uint32_t previous{invalid_scene_handle};
  std::uint32_t current{invalid_scene_handle};
}; // struct active_scene_changed_event

struct scene_loaded_event {
  std::uint32_t handle{0u};
  load_scene_mode mode{load_scene_mode::single};
}; // struct scene_loaded_event

/** @brief Which scenes are loaded and which is active, for the editor to restore after Play. */
struct scene_set {
  std::vector<loaded_scene> scenes{};
  std::uint32_t active{1u};
  std::uint32_t next_handle{2u};
}; // struct scene_set

/**
 * @brief Owns the world (one scene, one registry) and the set of loaded scenes in it.
 *
 * Loads are queued and applied at the start of the next frame (pre_update), never in the middle of a script, like Unity's LoadScene.
 * Systems react through on_scene_unloading() / on_scene_loaded().
 */
class scenes_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<assets::assets_module>;

  scenes_module();

  ~scenes_module();

  /** @brief Applies queued load operations. */
  auto pre_update() -> void;

  auto late_update() -> void;

  /**
   * @brief Queues loading a project-relative scene; applied next frame. A missing scene logs an error and changes nothing.
   *
   * The handle is reserved now; find_loaded_scene() finds it once the load has applied.
   */
  auto load_scene(const std::filesystem::path& path, load_scene_mode mode = load_scene_mode::single) -> queued_load;

  /** @brief Queues replacing every loaded scene (except the persistent one) with an empty scene holding a camera. */
  auto new_scene() -> queued_load;

  /**
   * @brief Loads in the background, like Unity's LoadSceneAsync: the file is parsed off the main thread, the assets it references are
   * requested and awaited, and the scene activates at the start of a frame once ready and allowed.
   *
   * Async loads activate in the order they were started; one held back by allow_scene_activation holds back those after it.
   * Environment maps load at activation, since their bake blocks.
   */
  auto load_scene_async(const std::filesystem::path& path, load_scene_mode mode = load_scene_mode::single) -> queued_load;

  /** @brief 0 to 1, like Unity's AsyncOperation.progress: 0.9 means loaded and waiting for activation; 1 once done. */
  [[nodiscard]] auto operation_progress(std::uint32_t operation) const -> std::float_t;

  /** @brief Holds a loaded async scene at 0.9 until allowed, e.g. while a loading screen finishes. */
  auto set_allow_scene_activation(std::uint32_t operation, bool allow) -> void;

  [[nodiscard]] auto allow_scene_activation(std::uint32_t operation) const -> bool;

  /** @brief Queues unloading one loaded scene, like Unity's UnloadSceneAsync. The last scene and the persistent one can't be unloaded. */
  auto unload_scene(std::uint32_t handle) -> std::uint32_t;

  /** @brief Whether a queued operation (by id from load_scene/new_scene/unload_scene) has been applied or rejected. */
  [[nodiscard]] auto is_operation_done(std::uint32_t operation) const -> bool;

  /** @brief Makes a loaded scene the one new top-level nodes join, right away. False for an unknown or persistent scene. */
  auto set_active_scene(std::uint32_t handle) -> bool;

  /** @brief Moves @p target's top-level node (with its subtree) into another loaded scene. False for an unknown scene. */
  auto move_node_to_scene(node target, std::uint32_t handle) -> bool;

  /** @brief Moves @p target's top-level node into the persistent scene, which single-mode loads keep. */
  auto dont_destroy_on_load(node target) -> void;

  /**
   * @brief Whether loads must be scenes from the project's scene list, like Unity builds. The runtime turns this on; the editor
   * can open any scene.
   */
  auto set_require_scene_list(bool required) noexcept -> void {
    _require_scene_list = required;
  }

  /** @brief Applies queued operations right away. Only outside script execution: editor actions and startup. */
  auto apply_pending_operations() -> void;

  [[nodiscard]] auto loaded_scenes() const noexcept -> const std::vector<loaded_scene>& {
    return _scene_set.scenes;
  }

  /** @brief The loaded scene new top-level nodes join. */
  [[nodiscard]] auto active_scene_handle() const noexcept -> std::uint32_t {
    return _scene_set.active;
  }

  [[nodiscard]] auto find_loaded_scene(std::uint32_t handle) const -> memory::observer_ptr<const loaded_scene>;

  /** @brief Records where a loaded scene was saved, e.g. after Save As. */
  auto set_scene_path(std::uint32_t handle, const std::filesystem::path& path) -> void;

  [[nodiscard]] auto current_scene_set() const -> const scene_set& {
    return _scene_set;
  }

  /** @brief Restores a captured scene set and drops queued operations; the world's nodes must already match it (e.g. a play-mode snapshot just loaded). */
  auto restore_scene_set(scene_set set) -> void;

  /** @brief Fires before a loaded scene's nodes are destroyed. */
  [[nodiscard]] auto on_scene_unloading() noexcept -> signals::signal<std::uint32_t>& {
    return _on_scene_unloading;
  }

  /** @brief Fires after a scene's nodes were destroyed, while its record is still listed. */
  [[nodiscard]] auto on_scene_unloaded() noexcept -> signals::signal<const loaded_scene&>& {
    return _on_scene_unloaded;
  }

  [[nodiscard]] auto on_active_scene_changed() noexcept -> signals::signal<const active_scene_changed_event&>& {
    return _on_active_scene_changed;
  }

  /** @brief Fires after a scene's nodes were added to the world. */
  [[nodiscard]] auto on_scene_loaded() noexcept -> signals::signal<const scene_loaded_event&>& {
    return _on_scene_loaded;
  }

  [[nodiscard]] auto active_scene() noexcept -> scene& {
    return _scene;
  }

  [[nodiscard]] auto active_scene() const noexcept -> const scene& {
    return _scene;
  }

  [[nodiscard]] auto is_simulating() const noexcept -> bool {
    return _is_simulating;
  }

  auto set_simulating(bool value) noexcept -> void {
    _is_simulating = value;
  }

  /**
   * @brief core::engine::delta_time() while simulating, else zero.
   *
   * Every time-integrating system should pull dt from here instead of core::engine::delta_time()
   * directly, so they all freeze together when simulation pauses.
   */
  [[nodiscard]] auto simulation_delta_time() const -> units::seconds {
    return _is_simulating ? core::engine::delta_time() : units::seconds{};
  }

  /**
   * @brief Accumulated simulation_delta_time() — core::engine::time() with pauses subtracted out.
   *
   * Stops advancing while simulation is paused, so absolute-time animation (shader scroll,
   * procedural motion, ...) freezes in sync instead of continuing. Updated once per frame in
   * late_update(), before render reads it.
   */
  [[nodiscard]] auto simulation_time() const -> units::seconds {
    return _simulation_time;
  }

  /** @brief Queues a destroyed instance_buffer's GPU buffer for retirement. Safe from any thread. */
  auto release_instance_buffer(graphics::buffer_handle handle) -> void;

  /** @brief Retires every buffer queued by release_instance_buffer since the last call. Render idle only. */
  auto collect_released_instance_buffers() -> void;

private:

  // Declared before _scene: the scene's instance_buffers still queue here while it is destroyed.
  std::mutex _released_instance_buffers_mutex{};
  std::vector<graphics::buffer_handle> _released_instance_buffers{};

  enum class operation_kind : std::uint8_t {
    load,
    unload
  }; // enum class operation_kind

  struct pending_operation {
    std::uint32_t id{0u};
    operation_kind kind{operation_kind::load};
    std::uint32_t handle{invalid_scene_handle}; // reserved for a load, the target of an unload
    std::filesystem::path path{}; // empty for new_scene
    load_scene_mode mode{load_scene_mode::single};
  }; // struct pending_operation

  auto _apply(const pending_operation& operation) -> void;

  auto _apply_load(const pending_operation& operation) -> void;

  [[nodiscard]] auto _is_loadable(const pending_operation& operation) const -> bool;

  // Adds a resolved snapshot to the world: the part of a load shared by sync and async loads.
  auto _activate_load(const pending_operation& operation, const YAML::Node& snapshot) -> void;

  struct async_load {
    pending_operation operation{};
    std::future<YAML::Node> parse{};
    YAML::Node snapshot{};
    bool parsed{false};
    bool failed{false};
    bool dependencies_ready{false};
    bool allow_activation{true};
    std::vector<std::function<bool()>> dependencies{}; // each holds its asset's handle, keeping it loaded until activation
    std::float_t progress{0.0f};
  }; // struct async_load

  // Polls parsing and asset readiness, then activates what's ready.
  auto _update_async_loads() -> void;

  // Activates async loads front to back while each is ready and allowed; with force, all of them regardless.
  auto _finish_async_loads(bool force) -> void;

  // Requests every asset a parsed snapshot's tables reference.
  auto _request_dependencies(async_load& pending) -> void;

  auto _apply_unload(const pending_operation& operation) -> void;

  // Makes @p handle active and announces it; also picks a camera if the world lost its own.
  auto _change_active(std::uint32_t handle) -> void;

  auto _ensure_persistent_scene() -> void;

  scene _scene{};
  scene_set _scene_set{{loaded_scene{1u, {}, "Scene"}}, 1u, 2u};
  std::vector<pending_operation> _pending_operations{};
  std::vector<async_load> _async_loads{};
  std::uint32_t _next_operation_id{1u};
  bool _require_scene_list{false};

  signals::signal<std::uint32_t> _on_scene_unloading{};
  signals::signal<const scene_loaded_event&> _on_scene_loaded{};
  signals::signal<const loaded_scene&> _on_scene_unloaded{};
  signals::signal<const active_scene_changed_event&> _on_active_scene_changed{};
  bool _is_simulating{true};
  units::seconds _simulation_time{};

}; // class scenes_module

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENES_MODULE_HPP_
