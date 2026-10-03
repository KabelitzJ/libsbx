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

/** @brief Converts a uuid to an asset picker item; built-in primitives have no manifest path, so they show their name. */
auto to_picker_item(const sbx::assets::assets_module& assets_module, const sbx::math::uuid& id) -> sbx::render::asset_picker_item;

/** @brief A material picker with an optional "Reset to Mesh Default" and a button to edit the material. allow_none offers "(None)", for optional script fields. */
auto draw_material_picker(editor_state& state, const char* popup_id, sbx::assets::material_handle& slot, sbx::assets::assets_module& assets_module, const sbx::assets::material_handle& mesh_default = {}, bool allow_none = false) -> bool;

/** @brief Forks a material into a new .material next to the mesh (or the assets root for a nil mesh_id), so edits don't affect other users of the original. */
auto extract_material_to_asset(sbx::assets::assets_module& assets_module, const sbx::assets::material_handle& source, const sbx::math::uuid& mesh_id) -> sbx::assets::material_handle;

/** @brief A texture picker with real thumbnails; format follows load_material's per-slot convention (sRGB for albedo/emissive, unorm otherwise). */
auto draw_texture_picker(editor_state& state, const char* popup_id, sbx::assets::texture2d_handle& slot, sbx::assets::assets_module& assets_module, sbx::graphics::format format) -> bool;

/** @brief A font picker for ui_text::font. */
auto draw_font_picker(editor_state& state, const char* popup_id, sbx::assets::font_handle& slot) -> bool;

/** @brief A mesh picker for mesh_renderer.mesh. The caller clears the materials on change so they reseed from the new mesh. */
auto draw_mesh_picker(editor_state& state, const char* popup_id, sbx::assets::mesh_handle& slot, sbx::assets::assets_module& assets_module) -> bool;

/** @brief A particle effect picker with a button to edit the effect. */
auto draw_particle_effect_picker(editor_state& state, const char* popup_id, sbx::assets::particle_effect_handle& slot, sbx::assets::assets_module& assets_module) -> bool;

/** @brief An animation graph picker; preview_mesh_id (nil if unknown) lets the graph editor list the mesh's real clip names. */
auto draw_animation_graph_picker(editor_state& state, const char* popup_id, sbx::assets::animation_graph_handle& slot, sbx::math::uuid preview_mesh_id) -> bool;

/** @brief A shader graph picker with a button to open the graph editor. */
auto draw_shader_graph_picker(editor_state& state, const char* popup_id, sbx::assets::shader_graph_handle& slot) -> bool;

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_ASSET_PICKERS_HPP_
