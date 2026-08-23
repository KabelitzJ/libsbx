// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_PROPERTIES_PANEL_HPP_
#define EDITOR_PANELS_PROPERTIES_PANEL_HPP_

#include <array>
#include <cstddef>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/particle_effect.hpp>

#include <libsbx/scenes/node.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief The "Properties" panel: an inspector for whatever editor_state's current selection is —
 * a scene node's name/transform/components (with add/remove-component controls), a material
 * asset's editable fields, other asset kinds' read-only summaries, or an empty-state message if
 * nothing is selected.
 */
class properties_panel final : public editor_panel {

public:

  auto draw(editor_state& state) -> void override;

private:

  // Caches the handle for whichever asset was selected most recently, so a load is only attempted
  // once per distinct selection rather than every frame — repeatedly calling load_*() per frame is
  // wasteful even when it hits assets_module's cache, and is actively harmful if a load ever fails:
  // without this, a failed load retries the full cook pipeline every single frame, forever.
  struct asset_property_cache {
    sbx::math::uuid id{sbx::math::uuid::nil()};
    sbx::assets::texture_handle texture{};
    sbx::assets::mesh_handle mesh{};
    sbx::assets::material_handle material{};
    sbx::assets::environment_map_handle environment_map{};
    sbx::assets::particle_effect_handle particle_effect{};
  }; // struct asset_property_cache

  auto _draw_node_properties(editor_state& state, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void;
  auto _draw_name_field(sbx::scenes::node& node) -> void;
  auto _draw_transform_section(sbx::scenes::node& node) -> void;
  auto _draw_asset_properties(const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;
  auto _draw_material_properties(const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;
  auto _draw_particle_effect_properties(const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void;

  // Editable name field: staged into a buffer, only re-synced from the node when the selection
  // changes (so mid-edit keystrokes aren't clobbered by re-reading the committed name).
  std::array<char, 128u> _name_buffer{};
  sbx::math::uuid _name_buffer_id{sbx::math::uuid::nil()};

  // Rotation is a quaternion, edited as Euler degrees. Re-deriving Euler from the quaternion every
  // frame is unstable near gimbal lock, so the working triplet is cached and only re-synced when
  // something other than this widget changed the quaternion (a reselect, or e.g. the gizmo).
  sbx::math::uuid _rotation_node_id{sbx::math::uuid::nil()};
  sbx::math::quaternion _rotation_cache{sbx::math::quaternion::identity};
  std::array<std::float_t, 3u> _rotation{0.0f, 0.0f, 0.0f};

  asset_property_cache _asset_cache{};

  // Staged edits for the selected material asset (case asset_kind::material in
  // _draw_asset_properties). Seeded from the loaded material whenever _asset_cache.id changes;
  // committed to the material record (and disk) only by an explicit Save button.
  sbx::assets::material::create_info _material_edit{};

  // Same idea as _material_edit, for asset_kind::particle_effect.
  sbx::assets::particle_effect::create_info _particle_effect_edit{};

}; // class properties_panel

} // namespace editor

#endif // EDITOR_PANELS_PROPERTIES_PANEL_HPP_
