// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_INSPECTOR_PANEL_HPP_
#define EDITOR_PANELS_INSPECTOR_PANEL_HPP_

#include <array>
#include <cstddef>
#include <optional>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/shader_graph.hpp>
#include <libsbx/assets/font.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief The "Inspector" panel: an inspector for whatever editor_state's current selection is —
 * a scene node's name/transform/components (with add/remove-component controls), a material
 * asset's editable fields, other asset kinds' read-only summaries, or an empty-state message if
 * nothing is selected.
 */
class inspector_panel final : public editor_panel {

public:

  /** @see hierarchy_panel::window_name */
  inline static constexpr auto window_name = ICON_MDI_INFORMATION " Inspector###inspector_panel";

  auto draw(editor_state& state) -> void override;

private:

  // Caches the handle for the most recently selected asset so load_*() is only attempted once per
  // selection, not every frame — without this a failed load retries the full cook pipeline forever.
  struct asset_property_cache {
    sbx::math::uuid id{sbx::math::uuid::nil()};
    sbx::assets::texture_handle texture{};
    sbx::assets::mesh_handle mesh{};
    sbx::assets::material_handle material{};
    sbx::assets::environment_map_handle environment_map{};
    sbx::assets::particle_effect_handle particle_effect{};
    sbx::assets::animation_graph_handle animation_graph{};
    sbx::assets::shader_graph_handle shader_graph{};
    sbx::assets::font_handle font{};
    sbx::assets::scene_handle scene{};
  }; // struct asset_property_cache

  /**
   * @brief draw_identity gates the name field + transform section — false for a prefab's own edit
   * view (_draw_prefab_edit below), since placement and Hierarchy label are per-instance concepts,
   * never prefab-shared content. Every other call site leaves it at the default.
   */
  auto _draw_node_properties(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module, bool draw_identity = true) -> void;
  auto _draw_active_checkbox(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
  auto _draw_name_field(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
  auto _draw_layer_field(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
  auto _draw_transform_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
  auto _draw_asset_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;
  auto _draw_material_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;
  auto _draw_particle_effect_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;

  /**
   * @brief Lets a prefab asset be edited directly, with no instance anywhere in the real scene:
   * deserializes the prefab's snapshot into a private scratch scene the Inspector owns, then draws
   * it through the exact same _draw_node_properties every real node uses (identity fields off —
   * see its doc comment) — reusing every per-component-type section and the add/remove-component
   * menu unmodified, since they already take their target scene as an explicit parameter (see
   * command.hpp's doc comment) rather than resolving one internally. No global state.
   *
   * Selection-driven, same pattern as _asset_cache: re-populated whenever the selected prefab's id
   * changes, not an explicitly opened/closed mode — selecting a `.prefab` tile goes straight here.
   */
  struct prefab_edit_session {
    sbx::scenes::scene scene{"Prefab Edit"};
    sbx::assets::prefab_handle prefab{};
    sbx::math::uuid root_id{sbx::math::uuid::nil()};
  }; // struct prefab_edit_session

  auto _draw_prefab_edit(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;

  /** @brief When node is a prefab instance, a small header above its usual component list: source prefab's name plus an "Update Prefab" button pushing this node's entire current subtree back to it. */
  auto _draw_prefab_instance_header(sbx::scenes::scene& target, sbx::scenes::node& node) -> void;

  std::optional<prefab_edit_session> _prefab_edit_session{};

  // Editable name field: staged into a buffer, only re-synced from the node when the selection
  // changes (so mid-edit keystrokes aren't clobbered by re-reading the committed name).
  std::array<char, 128u> _name_buffer{};
  sbx::math::uuid _name_buffer_id{sbx::math::uuid::nil()};
  std::optional<sbx::scenes::tag> _pending_name_before{};

  // Rotation is a quaternion edited as Euler degrees; re-deriving Euler every frame is unstable
  // near gimbal lock, so the triplet is cached and only re-synced when something else (reselect,
  // the gizmo) changed the quaternion.
  sbx::math::uuid _rotation_node_id{sbx::math::uuid::nil()};
  sbx::math::quaternion _rotation_cache{sbx::math::quaternion::identity};
  std::array<std::float_t, 3u> _rotation{0.0f, 0.0f, 0.0f};

  // Captured when a Position/Rotation/Scale drag starts, pushed as one modify_component_command
  // when it ends — one shared field covers all three since only one can be mid-drag at a time.
  std::optional<sbx::scenes::local_transform> _pending_transform_before{};

  asset_property_cache _asset_cache{};

  // Staged edits for the selected material (asset_kind::material). Seeded from the loaded material
  // whenever _asset_cache.id changes; committed only by an explicit Save button.
  sbx::assets::material::create_info _material_edit{};

  // Set when the Material Inspector's shader graph picker just (re)assigned a new graph -- that
  // graph loads asynchronously (see asset_residency::load_shader_graph), so its nodes/parameters()
  // are still empty on the very frame it's picked. Re-checked every frame afterward; once the graph
  // has actually finished loading, _material_edit's generic_params/generic_textures get seeded from
  // its own node defaults exactly once, then this clears (see _draw_material_properties).
  bool _shader_graph_seed_pending{false};

  // Same idea as _material_edit, for asset_kind::particle_effect.
  sbx::assets::particle_effect::create_info _particle_effect_edit{};

}; // class inspector_panel

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_PANEL_HPP_
