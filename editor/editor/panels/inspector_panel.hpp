// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_INSPECTOR_PANEL_HPP_
#define EDITOR_PANELS_INSPECTOR_PANEL_HPP_

#include <array>
#include <cstddef>
#include <optional>
#include <span>

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

/** @brief The Inspector: edits whatever is selected, from a node's transform and components to a material's fields, or shows an empty state. */
class inspector_panel final : public editor_panel {

public:

  // The panel's ImGui::Begin() string.
  inline static constexpr auto window_name = ICON_MDI_INFORMATION " Inspector###inspector_panel";

  auto draw(editor_state& state) -> void override;

private:

  // The selected asset's handle, so a failed load isn't retried every frame.
  struct asset_property_cache {
    sbx::math::uuid id{sbx::math::uuid::nil()};
    sbx::assets::texture2d_handle texture{};
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
   * @brief Draws a node's properties.
   *
   * A non-empty @p multi_selection shows only the layer, transform and shared components, with edits reaching every selected node; scripts, the prefab header and Add Component stay single-selection.
   *
   * @param state The editor state.
   * @param target The scene containing the node.
   * @param node The primary node.
   * @param assets_module The assets module.
   * @param draw_identity Whether to show the name and transform; false for prefab editing.
   * @param multi_selection Every selected node's id, or empty for a single selection.
   */
  auto _draw_node_properties(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module, bool draw_identity = true, std::span<const sbx::math::uuid> multi_selection = {}) -> void;
  auto _draw_active_checkbox(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
  auto _draw_name_field(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
  auto _draw_layer_field(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
  auto _draw_transform_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
  auto _draw_asset_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;
  auto _draw_material_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;
  auto _draw_particle_effect_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;

  // Edits a prefab directly: its snapshot is deserialized into a private scratch scene and drawn like any node. Repopulated whenever the selected prefab changes.
  struct prefab_edit_session {
    sbx::scenes::scene scene{"Prefab Edit"};
    sbx::assets::prefab_handle prefab{};
    sbx::math::uuid root_id{sbx::math::uuid::nil()};
  }; // struct prefab_edit_session

  auto _draw_prefab_edit(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;

  /** @brief For a prefab instance: the source prefab's name and an "Update Prefab" button that pushes this subtree back to it. */
  auto _draw_prefab_instance_header(sbx::scenes::scene& target, sbx::scenes::node& node) -> void;

  std::optional<prefab_edit_session> _prefab_edit_session{};

  // Staged name, re-synced only when the selection changes so typing isn't clobbered.
  std::array<char, 128u> _name_buffer{};
  sbx::math::uuid _name_buffer_id{sbx::math::uuid::nil()};
  std::optional<sbx::scenes::tag> _pending_name_before{};

  // Euler degrees cached per node: re-deriving from the quaternion every frame is unstable near gimbal lock.
  sbx::math::uuid _rotation_node_id{sbx::math::uuid::nil()};
  sbx::math::quaternion _rotation_cache{sbx::math::quaternion::identity};
  std::array<std::float_t, 3u> _rotation{0.0f, 0.0f, 0.0f};

  // The transform before a drag, pushed as one command when it ends; only one field can be mid-drag.
  std::optional<sbx::scenes::local_transform> _pending_transform_before{};

  asset_property_cache _asset_cache{};

  // Staged material edits, seeded when the selected asset changes and committed by Save.
  sbx::assets::material::create_info _material_edit{};

  bool _material_edit_pending{false};

  // Set when a shader graph is assigned: it loads asynchronously, so its defaults are seeded into _material_edit once it's loaded.
  bool _shader_graph_seed_pending{false};

  sbx::assets::particle_effect::create_info _particle_effect_edit{};

}; // class inspector_panel

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_PANEL_HPP_
