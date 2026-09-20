// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/inspector_asset_pickers.hpp>

#include <filesystem>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/primitive_meshes.hpp>

#include <editor/widgets/asset_path.hpp>

namespace editor {

auto to_picker_item(const sbx::assets::assets_module& assets_module, const sbx::math::uuid& id) -> sbx::render::asset_picker_item {
  if (id == sbx::math::uuid::nil()) {
    return {};
  }

  if (const auto kind = sbx::assets::primitive_mesh_kind_of(id); kind.has_value()) {
    return sbx::render::asset_picker_item{id, std::filesystem::path{sbx::assets::primitive_mesh_name(*kind)}};
  }

  return sbx::render::asset_picker_item{id, relative_asset_path(assets_module, id)};
}

auto draw_material_picker(editor_state& state, const char* popup_id, sbx::assets::material_handle& slot, sbx::assets::assets_module& assets_module, const sbx::assets::material_handle& mesh_default, bool allow_none) -> bool {
  const auto current = slot.is_valid() ? to_picker_item(assets_module, slot->id()) : sbx::render::asset_picker_item{};
  const auto default_item = mesh_default.is_valid() ? to_picker_item(assets_module, mesh_default->id()) : sbx::render::asset_picker_item{};

  const auto options = sbx::render::asset_picker_options{
    .kind = sbx::render::asset_picker_kind::material,
    .extensions = {".material"},
    .allow_none = allow_none,
    .show_edit_button = true,
    .show_reveal_button = true,
  };

  const auto result = sbx::render::draw_asset_picker(popup_id, current, default_item, options);

  if (result.cleared) {
    slot = sbx::assets::material_handle{};
  } else if (result.reset_to_default) {
    slot = mesh_default;
  } else if (result.changed) {
    // load_material(path) resolves relative against assets_directory() internally and reuses the
    // file's real uuid if it's already imported — calling import(relative) directly here would
    // mint a second, broken uuid keyed on an unresolved path.
    slot = assets_module.load_material(result.picked.path);
  }

  if (result.edit_requested && slot.is_valid()) {
    state.select_asset(slot->id(), relative_asset_path(assets_module, slot->id()), asset_kind::material);
  }

  if (result.reveal_requested && slot.is_valid()) {
    state.request_reveal_in_browser(relative_asset_path(assets_module, slot->id()));
  }

  return result.changed;
}

auto extract_material_to_asset(sbx::assets::assets_module& assets_module, const sbx::assets::material_handle& source, const sbx::math::uuid& mesh_id) -> sbx::assets::material_handle {
  auto& project = sbx::core::engine::project();

  const auto directory = relative_asset_path(assets_module, mesh_id).parent_path();

  auto info = sbx::assets::material::create_info{};
  info.name = source->name();
  info.base_color_factor = source->base_color_factor();
  info.emissive_factor = source->emissive_factor();
  info.metallic_factor = source->metallic_factor();
  info.roughness_factor = source->roughness_factor();
  info.alpha = source->alpha();
  info.alpha_cutoff = source->alpha_cutoff();
  info.is_double_sided = source->is_double_sided();
  info.casts_shadow = source->casts_shadow();
  info.receives_shadow = source->receives_shadow();
  info.normal_scale = source->normal_scale();
  info.occlusion_strength = source->occlusion_strength();
  info.emissive_strength = source->emissive_strength();
  info.ior = source->ior();
  info.albedo = source->albedo();
  info.normal = source->normal();
  info.metallic_roughness = source->metallic_roughness();
  info.occlusion = source->occlusion();
  info.emissive = source->emissive();

  auto file_name = info.name + ".material";
  auto suffix = 1;

  while (std::filesystem::exists(project.assets_directory() / directory / file_name)) {
    file_name = fmt::format("{} {}.material", info.name, suffix++);
  }

  auto handle = assets_module.create_material(info);
  assets_module.save_material(handle, directory / file_name);

  return handle;
}

auto draw_texture_picker(editor_state& state, const char* popup_id, sbx::assets::texture_handle& slot, sbx::assets::assets_module& assets_module, sbx::graphics::format format) -> bool {
  const auto current = slot.is_valid() ? to_picker_item(assets_module, slot->id()) : sbx::render::asset_picker_item{};

  const auto options = sbx::render::asset_picker_options{
    .kind = sbx::render::asset_picker_kind::texture,
    .extensions = {".png", ".jpg", ".jpeg"},
    .allow_none = true,
    .show_reveal_button = true,
    .load_format = format,
  };

  const auto result = sbx::render::draw_asset_picker(popup_id, current, {}, options);

  if (result.cleared) {
    slot = sbx::assets::texture_handle{};
  } else if (result.changed) {
    slot = assets_module.load_texture(result.picked.path, format);
  }

  if (result.reveal_requested && slot.is_valid()) {
    state.request_reveal_in_browser(relative_asset_path(assets_module, slot->id()));
  }

  return result.changed;
}

auto draw_font_picker(editor_state& state, const char* popup_id, sbx::assets::font_handle& slot) -> bool {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto current = slot.is_valid() ? to_picker_item(assets_module, slot->id()) : sbx::render::asset_picker_item{};

  const auto options = sbx::render::asset_picker_options{
    .kind = sbx::render::asset_picker_kind::font,
    .extensions = {".ttf"},
    .allow_none = true,
    .show_reveal_button = true,
  };

  const auto result = sbx::render::draw_asset_picker(popup_id, current, {}, options);

  if (result.cleared) {
    slot = sbx::assets::font_handle{};
  } else if (result.changed) {
    slot = assets_module.load_font(result.picked.path);
  }

  if (result.reveal_requested && slot.is_valid()) {
    state.request_reveal_in_browser(relative_asset_path(assets_module, slot->id()));
  }

  return result.changed;
}

auto draw_mesh_picker(editor_state& state, const char* popup_id, sbx::assets::mesh_handle& slot, sbx::assets::assets_module& assets_module) -> bool {
  const auto current = slot.is_valid() ? to_picker_item(assets_module, slot->id()) : sbx::render::asset_picker_item{};

  const auto options = sbx::render::asset_picker_options{
    .kind = sbx::render::asset_picker_kind::mesh,
    .extensions = {".gltf", ".glb"},
    .show_edit_button = true,
    .show_reveal_button = true,
    .show_builtin_primitives = true,
  };

  const auto result = sbx::render::draw_asset_picker(popup_id, current, {}, options);

  if (result.changed) {
    if (const auto kind = sbx::assets::primitive_mesh_kind_of(result.picked.id); kind.has_value()) {
      slot = assets_module.load_mesh(result.picked.id);
    } else {
      slot = assets_module.load_mesh(result.picked.path);
    }
  }

  if (result.edit_requested && slot.is_valid() && !sbx::assets::primitive_mesh_kind_of(slot->id()).has_value()) {
    state.select_asset(slot->id(), relative_asset_path(assets_module, slot->id()), asset_kind::mesh);
  }

  if (result.reveal_requested && slot.is_valid() && !sbx::assets::primitive_mesh_kind_of(slot->id()).has_value()) {
    state.request_reveal_in_browser(relative_asset_path(assets_module, slot->id()));
  }

  return result.changed;
}

auto draw_particle_effect_picker(editor_state& state, const char* popup_id, sbx::assets::particle_effect_handle& slot, sbx::assets::assets_module& assets_module) -> bool {
  const auto current = slot.is_valid() ? to_picker_item(assets_module, slot->id()) : sbx::render::asset_picker_item{};

  const auto options = sbx::render::asset_picker_options{
    .kind = sbx::render::asset_picker_kind::particle_effect,
    .extensions = {".particle_effect"},
    .allow_none = true,
    .show_edit_button = true,
    .show_reveal_button = true,
  };

  const auto result = sbx::render::draw_asset_picker(popup_id, current, {}, options);

  if (result.cleared) {
    slot = sbx::assets::particle_effect_handle{};
  } else if (result.changed) {
    slot = assets_module.load_particle_effect(result.picked.path);
  }

  if (result.edit_requested && slot.is_valid()) {
    state.select_asset(slot->id(), relative_asset_path(assets_module, slot->id()), asset_kind::particle_effect);
  }

  if (result.reveal_requested && slot.is_valid()) {
    state.request_reveal_in_browser(relative_asset_path(assets_module, slot->id()));
  }

  return result.changed;
}

auto draw_animation_graph_picker(editor_state& state, const char* popup_id, sbx::assets::animation_graph_handle& slot, sbx::math::uuid preview_mesh_id) -> bool {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto current = slot.is_valid() ? to_picker_item(assets_module, slot->id()) : sbx::render::asset_picker_item{};

  const auto options = sbx::render::asset_picker_options{
    .kind = sbx::render::asset_picker_kind::animation_graph,
    .extensions = {".animation_graph"},
    .allow_none = true,
    .show_edit_button = true,
    .show_reveal_button = true,
  };

  const auto result = sbx::render::draw_asset_picker(popup_id, current, {}, options);

  if (result.cleared) {
    slot = sbx::assets::animation_graph_handle{};
  } else if (result.changed) {
    slot = assets_module.load_animation_graph(result.picked.path);
  }

  if (result.edit_requested && slot.is_valid()) {
    // Jumps straight into the visual graph editor rather than just selecting it (select_asset's
    // read-only Inspector summary) -- the common case here is "assigned a graph, now go build it".
    state.request_open_animation_graph_editor(slot->id(), relative_asset_path(assets_module, slot->id()), preview_mesh_id);
  }

  if (result.reveal_requested && slot.is_valid()) {
    state.request_reveal_in_browser(relative_asset_path(assets_module, slot->id()));
  }

  return result.changed;
}

auto draw_shader_graph_picker(editor_state& state, const char* popup_id, sbx::assets::shader_graph_handle& slot) -> bool {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto current = slot.is_valid() ? to_picker_item(assets_module, slot->id()) : sbx::render::asset_picker_item{};

  const auto options = sbx::render::asset_picker_options{
    .kind = sbx::render::asset_picker_kind::shader_graph,
    .extensions = {".shadergraph"},
    .allow_none = true,
    .show_edit_button = true,
    .show_reveal_button = true,
  };

  const auto result = sbx::render::draw_asset_picker(popup_id, current, {}, options);

  if (result.cleared) {
    slot = sbx::assets::shader_graph_handle{};
  } else if (result.changed) {
    slot = assets_module.load_shader_graph(result.picked.path);
  }

  if (result.edit_requested && slot.is_valid()) {
    state.request_open_shader_graph_editor(slot->id(), relative_asset_path(assets_module, slot->id()));
  }

  if (result.reveal_requested && slot.is_valid()) {
    state.request_reveal_in_browser(relative_asset_path(assets_module, slot->id()));
  }

  return result.changed;
}

} // namespace editor
