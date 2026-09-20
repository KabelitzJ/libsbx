// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_INSPECTOR_ASSET_PICKERS_HPP_
#define EDITOR_PANELS_INSPECTOR_ASSET_PICKERS_HPP_

#include <libsbx/math/uuid.hpp>

#include <libsbx/graphics/types.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/shader_graph.hpp>
#include <libsbx/assets/font.hpp>

#include <libsbx/render/ui/widgets/asset_picker.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Bridges a handle's uuid to the generic asset_picker widget's item type (uuid +
 * project-relative path). A built-in primitive mesh has no manifest entry --
 * relative_asset_path would come back empty and the picker button would show "(None)"
 * for an assigned primitive -- so its display "path" is just its name instead.
 */
auto to_picker_item(const sbx::assets::assets_module& assets_module, const sbx::math::uuid& id) -> sbx::render::asset_picker_item;

/**
 * @brief Thumbnail/icon button + searchable, thumbnail-rendered popup (sbx::render::asset_picker),
 * plus an optional "Reset to Mesh Default" (reseeds from the mesh's own submesh material). Also a
 * drag-and-drop target for a .material tile dragged straight from the Asset Browser. Second button
 * jumps Properties to that material's editable view. allow_none offers a "(None)" entry that clears
 * the slot -- off by default, since a mesh_renderer submesh always wants some material assigned;
 * a nullable script field (see inspector_script_section.cpp) passes true.
 */
auto draw_material_picker(editor_state& state, const char* popup_id, sbx::assets::material_handle& slot, sbx::assets::assets_module& assets_module, const sbx::assets::material_handle& mesh_default = {}, bool allow_none = false) -> bool;

/** @brief Forks a material into a new, independent .material asset next to the mesh, so editing the copy doesn't affect other nodes sharing the original. mesh_id may be nil (falls back to assets root). */
auto extract_material_to_asset(sbx::assets::assets_module& assets_module, const sbx::assets::material_handle& source, const sbx::math::uuid& mesh_id) -> sbx::assets::material_handle;

/**
 * @brief Same idea as draw_material_picker, for texture slots -- real GPU thumbnails in both the
 * closed button and the popup list (see asset_tile.hpp). format follows load_material's per-slot
 * convention (srgb for albedo/emissive, unorm for normal/metallic_roughness/occlusion).
 */
auto draw_texture_picker(editor_state& state, const char* popup_id, sbx::assets::texture_handle& slot, sbx::assets::assets_module& assets_module, sbx::graphics::format format) -> bool;

/** @brief Same idea as draw_texture_picker, for a font asset slot (ui_text::font). */
auto draw_font_picker(editor_state& state, const char* popup_id, sbx::assets::font_handle& slot) -> bool;

/**
 * @brief Same idea as draw_material_picker, for mesh_renderer.mesh. Doesn't touch renderer.materials
 * itself -- the caller detects the change and clears it so sync_materials_with_mesh reseeds cleanly
 * from the new mesh's submeshes.
 */
auto draw_mesh_picker(editor_state& state, const char* popup_id, sbx::assets::mesh_handle& slot, sbx::assets::assets_module& assets_module) -> bool;

/**
 * @brief Same idea as draw_mesh_picker, for particle_effect.effect -- same jump-to-edit button,
 * since particle_effect assets are edited in place (see inspector_panel::_draw_particle_effect_properties)
 * like materials.
 */
auto draw_particle_effect_picker(editor_state& state, const char* popup_id, sbx::assets::particle_effect_handle& slot, sbx::assets::assets_module& assets_module) -> bool;

/**
 * @brief Same idea as draw_particle_effect_picker, for animator.graph. preview_mesh_id (nil if
 * unknown) is forwarded to the graph editor so it can list the mesh's real clip names instead of
 * leaving animation_state::clip_name a free-text field -- see animation_graph_panel's doc comment.
 */
auto draw_animation_graph_picker(editor_state& state, const char* popup_id, sbx::assets::animation_graph_handle& slot, sbx::math::uuid preview_mesh_id) -> bool;

/**
 * @brief Same idea as draw_animation_graph_picker, for material::shader_graph -- jumps into
 * shader_graph_panel instead. No preview_mesh_id equivalent (a shader graph carries no mesh-shaped
 * reference the way a state's clip_name does).
 */
auto draw_shader_graph_picker(editor_state& state, const char* popup_id, sbx::assets::shader_graph_handle& slot) -> bool;

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_ASSET_PICKERS_HPP_
