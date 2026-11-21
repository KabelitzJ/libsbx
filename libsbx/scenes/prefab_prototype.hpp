// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_PREFAB_PROTOTYPE_HPP_
#define LIBSBX_SCENES_PREFAB_PROTOTYPE_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/type_list.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/prefab.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/character_controller.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>

#include <libsbx/canvas/components.hpp>

namespace sbx::scenes {

/**
 * @brief The components a prefab instance copies from its prototype: everything the scene serializer reads back, plus the skeleton_pose it derives from a skinned mesh.
 *
 * A node's identity (id, tag, relationship) is never copied; each instance gets its own.
 * A component the serializer writes but this list leaves out would silently vanish from spawned instances; check_prefab_copy_coverage reports such a component when a prototype is built.
 */
using prefab_copied_components = utility::type_list<
  local_transform,
  layer,
  inactive,
  mesh_renderer,
  skeleton_pose,
  animator,
  camera,
  directional_light,
  point_light,
  spot_light,
  decal,
  reflection_probe,
  skybox,
  particle_effect,
  audio_source,
  audio_listener,
  script_component,
  prefab_member,
  prefab_instance,
  physics::rigidbody,
  physics::shape_collider,
  physics::mesh_collider,
  physics::character_controller,
  physics::nav_agent,
  canvas::canvas,
  canvas::canvas_group,
  canvas::canvas_scaler,
  canvas::rect_transform,
  canvas::ui_image,
  canvas::ui_text,
  canvas::ui_button,
  canvas::ui_toggle,
  canvas::ui_slider,
  canvas::ui_scrollbar,
  canvas::ui_scroll_rect,
  canvas::ui_mask,
  canvas::layout_element,
  canvas::content_size_fitter,
  canvas::horizontal_layout_group,
  canvas::vertical_layout_group,
  canvas::grid_layout_group
>;

/** @brief The scene serializer's "type" key of every component in prefab_copied_components (skeleton_pose has none: it's derived from the mesh). */
inline constexpr auto prefab_copied_component_keys = std::array<std::string_view, 40u>{
  "transform", "layer", "inactive", "static_mesh", "animator", "camera", "directional_light", "point_light", "spot_light", "decal", "reflection_probe", "skybox", "particle_effect", "audio_source", "audio_listener", "script", "prefab_member", "prefab_instance",
  "rigidbody", "shape_collider", "mesh_collider", "character_controller", "nav_agent",
  "canvas", "canvas_group", "canvas_scaler", "rect_transform", "ui_image", "ui_text", "ui_button", "ui_toggle", "ui_slider", "ui_scrollbar", "ui_scroll_rect", "ui_mask", "layout_element", "content_size_fitter", "horizontal_layout_group", "vertical_layout_group", "grid_layout_group"
};

/**
 * @brief Copies every component of prefab_copied_components that @p from has onto @p to, replacing what @p to already has (its default transform and layer).
 *
 * @tparam Components The component types to copy.
 *
 * @param from The prototype node.
 * @param to The new instance node.
 */
template<typename... Components>
auto copy_prefab_components(const node& from, node& to, utility::type_list<Components...>) -> void {
  const auto copy = [&]<typename Component>() {
    if (!from.has_component<Component>()) {
      return;
    }

    // Empty marker types (inactive) have no value to copy, only presence.
    if constexpr (std::is_empty_v<Component>) {
      static_cast<void>(to.get_or_add_component<Component>());
    } else {
      to.get_or_add_component<Component>() = from.get_component<Component>();
    }
  };

  (copy.template operator()<Components>(), ...);
}

/**
 * @brief Copies every prefab-copied component of @p from onto @p to.
 *
 * @param from The prototype node.
 * @param to The new instance node.
 */
inline auto copy_prefab_components(const node& from, node& to) -> void {
  copy_prefab_components(from, to, prefab_copied_components{});
}

/**
 * @brief Points the references one node holds to other nodes of the same prefab (a canvas's camera, a toggle's group, a scroll rect's content, script fields typed Node) at their counterparts in the new instance.
 *
 * Run once every node of the instance exists; references to nodes outside the prefab are left as they are.
 *
 * @param instance_node A node of the new instance, with its components already copied.
 * @param remap Prototype node id to instance node id, for every node of the prefab.
 */
auto remap_prefab_node_references(node& instance_node, const std::unordered_map<math::uuid, math::uuid>& remap) -> void;

/**
 * @brief The component types in one serialized node that prefab instantiation wouldn't copy.
 *
 * @param node_yaml A node entry of a prefab snapshot (scene file format).
 *
 * @return The "type" keys not in prefab_copied_component_keys; empty when every component is covered.
 */
[[nodiscard]] auto uncopied_prefab_component_keys(const YAML::Node& node_yaml) -> std::vector<std::string>;

/**
 * @brief Logs an error for every component of a prefab snapshot that prefab instantiation wouldn't copy, so a component added to the serializer but not to prefab_copied_components is caught on first use.
 *
 * @param prefab_name The prefab's name, for the message.
 * @param snapshot The prefab's snapshot.
 *
 * @return Whether every component is covered.
 */
auto check_prefab_copy_coverage(std::string_view prefab_name, const YAML::Node& snapshot) -> bool;

/**
 * @brief One prefab version instantiated once from its YAML snapshot into a private scene, for instances to copy from instead of parsing the snapshot again.
 *
 * The scene is never updated, simulated or seen by any system, so its runtime-only state stays exactly what a fresh load leaves, and copies of it match what the YAML path would build.
 */
struct prefab_prototype {
  std::weak_ptr<assets::prefab> source{}; // the prefab record it was built from; expired once that record is gone
  std::uint64_t generation{0u};
  std::unique_ptr<scene> nodes_scene{};
  math::uuid root_id{math::uuid::nil()};
  std::vector<math::uuid> node_ids{}; // every node of the subtree, parents before children
}; // struct prefab_prototype

/**
 * @brief The prototypes of the prefabs instantiated so far, each rebuilt when its prefab changes (a new generation) and dropped once its prefab record is gone.
 *
 * Main thread only. Owned by scenes_module; usable without a running engine for prefabs whose snapshots need no assets.
 */
class prefab_prototype_cache {

public:

  /**
   * @brief The prototype of a prefab's current version, built from its snapshot on first use and after every change.
   *
   * @param prefab The prefab.
   *
   * @return The prototype; its nodes_scene is null for an invalid prefab or an empty snapshot.
   */
  [[nodiscard]] auto prototype_of(const assets::prefab_handle& prefab) -> const prefab_prototype&;

  /** @brief Drops the prototypes whose prefab record is gone, releasing the assets they hold. */
  auto prune() -> void;

  /** @brief Drops every prototype. */
  auto clear() -> void;

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return _prototypes.size();
  }

private:

  [[nodiscard]] static auto _build(const assets::prefab_handle& prefab) -> prefab_prototype;

  // Keyed by the prefab record: unsaved prefabs have no id yet, and a record outlives every handle to it.
  std::unordered_map<const assets::prefab*, prefab_prototype> _prototypes{};

}; // class prefab_prototype_cache

/**
 * @brief Instantiates a prefab's current version into a scene by copying its prototype, without parsing its snapshot.
 *
 * Matches scene_serializer::instantiate_prefab: every node gets a fresh id (@p root_id, if given, is forced onto the root, e.g. for an undo that recreates the same instance), keeps its prefab_member, and the root gets prefab_instance{prefab, its generation}.
 * Unlike the YAML path, references between the prefab's own nodes (a canvas's camera, a toggle's group, a scroll rect's content, script fields typed Node) point at the new instance's nodes.
 * The subtree is left under @p target's root; the caller repositions or reparents it.
 *
 * @param target The scene to instantiate into.
 * @param prefab The prefab.
 * @param cache The prototypes to copy from; the prefab's is built on first use.
 * @param root_id An id to give the root instead of a fresh one.
 *
 * @return The instance's root, or an invalid node for an invalid or empty prefab.
 */
auto instantiate_prefab_prototype(scene& target, const assets::prefab_handle& prefab, prefab_prototype_cache& cache, std::optional<math::uuid> root_id = std::nullopt) -> node;

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_PREFAB_PROTOTYPE_HPP_
