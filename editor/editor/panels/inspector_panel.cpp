// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/inspector_panel.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <type_traits>
#include <variant>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scripting/scripting_module.hpp>

#include <editor/commands/component_commands.hpp>
#include <editor/commands/script_commands.hpp>

#include <editor/panels/inspector_component_registry.hpp>
#include <editor/panels/inspector_script_section.hpp>

#include <editor/widgets/vector_fields.hpp>
#include <editor/widgets/layer_fields.hpp>

namespace editor {

auto inspector_panel::_draw_active_checkbox(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto active = node.is_active();

  if (ImGui::Checkbox("##Active", &active)) {
    if (active) {
      state.push_command(target, std::make_unique<remove_component_command<sbx::scenes::inactive>>(node.id(), sbx::scenes::inactive{}, "Activate Node"));
    } else {
      state.push_command(target, std::make_unique<add_component_command<sbx::scenes::inactive>>(node.id(), "Deactivate Node"));
    }
  }

  ImGui::SameLine();
}

auto inspector_panel::_draw_name_field(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  const auto id = node.id();

  if (id.value() != _name_buffer_id.value()) {
    const auto& current_name = node.name();
    std::strncpy(_name_buffer.data(), current_name.c_str(), _name_buffer.size() - 1u);
    _name_buffer[_name_buffer.size() - 1u] = '\0';
    _name_buffer_id = id;
  }

  if (ImGui::InputText("Name", _name_buffer.data(), _name_buffer.size())) {
    // Live-edited into the buffer; committed below once editing finishes.
  }

  if (ImGui::IsItemActivated() && !_pending_name_before) {
    _pending_name_before = node.name();
  }

  if (ImGui::IsItemDeactivatedAfterEdit()) {
    // scene::find(name) can go stale after this (scene::_entities_by_name is populated at creation
    // only) — fine, selection/hierarchy key on entity/id, never name.
    node.name() = sbx::scenes::tag{std::string{_name_buffer.data()}};

    if (_pending_name_before) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::tag>>(id, *_pending_name_before, node.name(), "Rename Node"));
      _pending_name_before.reset();
    }
  }

  ImGui::Text("UUID: %llu", static_cast<unsigned long long>(id.value()));
}

auto inspector_panel::_draw_layer_field(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  const auto before = node.layer();
  auto index = before.index;

  if (draw_layer_combo(state, "Layer", index)) {
    node.layer().index = index;
    state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::layer>>(node.id(), before, node.layer(), "Change Layer"));
  }
}

auto inspector_panel::_draw_transform_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  ImGui::SeparatorText("Transform");

  auto& transform = node.transform();

  // started captures the pre-mutation snapshot (must run before this frame's change, if any, is
  // applied below — the same frame can both start and finish a drag, via the reset button).
  // committed pushes it once the drag (or reset click) is done.
  const auto capture_before = [&](const vector3_edit_result& result) {
    if (result.started && !_pending_transform_before) {
      _pending_transform_before = transform;
    }
  };

  const auto commit_after = [&](const vector3_edit_result& result) {
    if (result.committed && _pending_transform_before) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::local_transform>>(node.id(), *_pending_transform_before, transform, "Edit Transform"));
      _pending_transform_before.reset();
    }
  };

  auto position = std::array<std::float_t, 3u>{transform.position.x(), transform.position.y(), transform.position.z()};
  const auto position_result = draw_vector3_control("Position", position, 0.0f, 0.05f);

  capture_before(position_result);

  if (position_result.changed) {
    transform.position = sbx::math::vector3{position[0], position[1], position[2]};
  }

  commit_after(position_result);

  // See _rotation_node_id/_rotation_cache/_rotation's declarations for why this is cached rather
  // than re-derived from the quaternion every frame.
  if (node.id().value() != _rotation_node_id.value() || !(transform.rotation == _rotation_cache)) {
    const auto euler = sbx::math::quaternion::euler_angles(transform.rotation);
    _rotation = {euler.x(), euler.y(), euler.z()};
    _rotation_node_id = node.id();
    _rotation_cache = transform.rotation;
  }

  const auto rotation_result = draw_vector3_control("Rotation", _rotation, 0.0f, 0.5f);

  capture_before(rotation_result);

  if (rotation_result.changed) {
    transform.rotation = sbx::math::quaternion{sbx::math::vector3{_rotation[0], _rotation[1], _rotation[2]}};
    _rotation_cache = transform.rotation;
  }

  commit_after(rotation_result);

  auto scale = std::array<std::float_t, 3u>{transform.scale.x(), transform.scale.y(), transform.scale.z()};
  const auto scale_result = draw_vector3_control("Scale", scale, 1.0f, 0.05f);

  capture_before(scale_result);

  if (scale_result.changed) {
    transform.scale = sbx::math::vector3{scale[0], scale[1], scale[2]};
  }

  commit_after(scale_result);
}

auto inspector_panel::_draw_node_properties(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module, bool draw_identity) -> void {
  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  // A little vertical breathing room between each section, on top of the frame/item padding
  // pushed in draw() — keeps a node with several components/scripts from reading as one dense
  // unbroken block of controls.
  const auto section_gap = [] { ImGui::Dummy(ImVec2{0.0f, 6.0f}); };

  if (draw_identity) {
    _draw_active_checkbox(state, target, node);
    _draw_name_field(state, target, node);
    _draw_layer_field(state, target, node);
    section_gap();
  }

  if (node.has_component<sbx::scenes::prefab_instance>()) {
    _draw_prefab_instance_header(target, node);
    section_gap();
  }

  if (draw_identity) {
    _draw_transform_section(state, target, node);
  }

  // Driven by component_entries() (see inspector_component_registry.hpp) rather than one
  // hand-written has_component<T>() check per type -- the same table also drives
  // draw_add_component_menu below, so a component type is registered in exactly one place.
  for (const auto& entry : component_entries()) {
    if (entry.has(node)) {
      section_gap();
      entry.draw(state, target, node, assets_module);
    }
  }

  if (node.has_component<sbx::scenes::script_component>()) {
    auto& scripts = node.get_component<sbx::scenes::script_component>();
    auto pending_removal = std::optional<std::string>{};

    for (auto index = std::size_t{0u}; index < scripts.scripts.size(); ++index) {
      section_gap();

      ImGui::PushID(static_cast<std::int32_t>(index));
      draw_script_section(state, target, node, scripts.scripts[index], pending_removal);
      ImGui::PopID();
    }

    if (pending_removal) {
      for (const auto& entry : scripts.scripts) {
        if (entry.class_name == *pending_removal) {
          state.push_command(target, std::make_unique<detach_script_command>(node.id(), entry));
          break;
        }
      }
    }
  }

  section_gap();
  ImGui::Separator();
  ImGui::Spacing();
  draw_add_component_menu(state, target, node, scripting_module);
}

auto inspector_panel::_draw_asset_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void {
  if (_asset_cache.id.value() != asset.id.value()) {
    _asset_cache = asset_property_cache{};
    _asset_cache.id = asset.id;

    switch (asset.kind) {
      case asset_kind::texture: _asset_cache.texture = assets_module.load_texture(asset.id); break;
      case asset_kind::mesh: _asset_cache.mesh = assets_module.load_mesh(asset.id); break;
      case asset_kind::material: _asset_cache.material = assets_module.load_material(asset.id); break;
      case asset_kind::environment_map: _asset_cache.environment_map = assets_module.load_environment_map(asset.id); break;
      case asset_kind::particle_effect: _asset_cache.particle_effect = assets_module.load_particle_effect(asset.id); break;
      case asset_kind::animation_graph: _asset_cache.animation_graph = assets_module.load_animation_graph(asset.id); break;
      case asset_kind::font: _asset_cache.font = assets_module.load_font(asset.id); break;
      case asset_kind::prefab:
      case asset_kind::scene:
      case asset_kind::script:
      case asset_kind::unknown:
        break;
    }

    if (asset.kind == asset_kind::particle_effect && _asset_cache.particle_effect.is_valid()) {
      const auto& effect = *_asset_cache.particle_effect;

      _particle_effect_edit.name = effect.name();
      _particle_effect_edit.emitters = effect.emitters();
    }

    if (asset.kind == asset_kind::material && _asset_cache.material.is_valid()) {
      const auto& material = *_asset_cache.material;

      _material_edit.name = material.name();
      _material_edit.base_color_factor = material.base_color_factor();
      _material_edit.emissive_factor = material.emissive_factor();
      _material_edit.metallic_factor = material.metallic_factor();
      _material_edit.roughness_factor = material.roughness_factor();
      _material_edit.alpha = material.alpha();
      _material_edit.shading = material.shading();
      _material_edit.alpha_cutoff = material.alpha_cutoff();
      _material_edit.is_double_sided = material.is_double_sided();
      _material_edit.casts_shadow = material.casts_shadow();
      _material_edit.receives_shadow = material.receives_shadow();
      _material_edit.normal_scale = material.normal_scale();
      _material_edit.occlusion_strength = material.occlusion_strength();
      _material_edit.emissive_strength = material.emissive_strength();
      _material_edit.ior = material.ior();
      _material_edit.uv_tiling = material.uv_tiling();
      _material_edit.uv_offset = material.uv_offset();
      _material_edit.albedo = material.albedo();
      _material_edit.normal = material.normal();
      _material_edit.metallic_roughness = material.metallic_roughness();
      _material_edit.occlusion = material.occlusion();
      _material_edit.emissive = material.emissive();
    }
  }

  ImGui::Text("Path: %s", asset.path.string().c_str());
  ImGui::Text("UUID: %llu", static_cast<unsigned long long>(asset.id.value()));

  if (ImGui::Button(ICON_MDI_FOLDER_SEARCH_OUTLINE " Show in Browser")) {
    state.request_reveal_in_browser(asset.path);
  }

  switch (asset.kind) {
    case asset_kind::texture: {
      ImGui::Text("Type: Texture");
      const auto& handle = _asset_cache.texture;
      if (handle.is_valid()) {
        ImGui::Text("Bindless Index: %u", handle->index());
        ImGui::Text("Resident: %s", assets_module.is_resident(handle) ? "yes" : "no");
      }
      break;
    }
    case asset_kind::mesh: {
      ImGui::Text("Type: Mesh");
      const auto& handle = _asset_cache.mesh;
      if (handle.is_valid()) {
        ImGui::Text("Submeshes: %zu", handle->submeshes().size());
        ImGui::Text("Uploaded: %s", handle->is_uploaded() ? "yes" : "no");
      }
      break;
    }
    case asset_kind::material: {
      ImGui::Text("Type: Material");
      _draw_material_properties(state, asset, assets_module);
      break;
    }
    case asset_kind::environment_map: {
      ImGui::Text("Type: Environment Map");
      const auto& handle = _asset_cache.environment_map;
      if (handle.is_valid()) {
        const auto is_baked = handle->radiance_index() != sbx::assets::environment_map::invalid_index &&
                               handle->irradiance_index() != sbx::assets::environment_map::invalid_index &&
                               handle->prefiltered_index() != sbx::assets::environment_map::invalid_index;
        ImGui::Text("Baked: %s", is_baked ? "yes" : "no");
        ImGui::Text("Prefiltered Mips: %u", handle->prefiltered_mip_count());
      }
      break;
    }
    case asset_kind::particle_effect: {
      ImGui::Text("Type: Particle Effect");
      _draw_particle_effect_properties(state, asset, assets_module);
      break;
    }
    case asset_kind::animation_graph: {
      ImGui::Text("Type: Animation Graph");

      if (ImGui::Button(ICON_MDI_STATE_MACHINE " Open Graph Editor")) {
        state.request_open_animation_graph_editor(asset.id, asset.path);
      }

      // The rest of this section is read-only -- states/transitions are edited in the graph
      // editor opened above, this is just a quick-glance summary.
      const auto& handle = _asset_cache.animation_graph;

      if (handle.is_valid()) {
        ImGui::Text("States: %zu", handle->states().size());
        ImGui::Text("Transitions: %zu", handle->transitions().size());

        const auto& states = handle->states();
        const auto entry_state_it = std::ranges::find(states, handle->entry_state_id(), &sbx::assets::animation_state::id);
        ImGui::Text("Entry State: %s", entry_state_it != states.end() ? entry_state_it->name.c_str() : "(none)");

        if (!handle->parameters().empty() && ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
          for (const auto& parameter : handle->parameters()) {
            std::visit([&parameter](const auto& value) {
              using value_type = std::decay_t<decltype(value)>;

              if constexpr (std::is_same_v<value_type, std::float_t>) {
                ImGui::Text("%s (Float): %.3f", parameter.name.c_str(), static_cast<std::double_t>(value));
              } else if constexpr (std::is_same_v<value_type, bool>) {
                ImGui::Text("%s (Bool): %s", parameter.name.c_str(), value ? "true" : "false");
              } else if constexpr (std::is_same_v<value_type, std::int32_t>) {
                ImGui::Text("%s (Int): %d", parameter.name.c_str(), value);
              } else {
                ImGui::Text("%s (Trigger)", parameter.name.c_str());
              }
            }, parameter.default_value);
          }

          ImGui::TreePop();
        }
      }

      break;
    }
    case asset_kind::font: {
      ImGui::Text("Type: Font");
      const auto& handle = _asset_cache.font;
      if (handle.is_valid()) {
        ImGui::Text("Bindless Index: %u", handle->atlas()->index());
        ImGui::Text("Resident: %s", assets_module.is_resident(handle) ? "yes" : "no");
      }
      break;
    }
    case asset_kind::prefab: {
      // Unreachable in practice -- draw() routes a selected prefab asset to _draw_prefab_edit
      // before this function is ever called for one. Kept only so this switch stays exhaustive.
      break;
    }
    case asset_kind::scene: {
      ImGui::Text("Type: Scene (not imported)");
      break;
    }
    case asset_kind::script: {
      ImGui::Text("Type: Script");
      ImGui::Text("Class: %s", asset.path.stem().string().c_str());
      ImGui::TextDisabled("Attach to a node via its \"Add Component > Script\" menu.");
      break;
    }
    case asset_kind::unknown: {
      ImGui::Text("Type: Unknown");
      break;
    }
  }
}

auto inspector_panel::draw(editor_state& state) -> void {
  // A bit more breathing room than ImGui's tight defaults. WindowPadding must be pushed before
  // Begin() -- it's read while laying out the window itself.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{10.0f, 10.0f});

  ImGui::Begin(window_name);

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{6.0f, 4.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{8.0f, 6.0f});

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (std::holds_alternative<node_selection>(state.current_selection)) {
    if (state.selected_node_count() > 1u) {
      ImGui::TextDisabled("%zu objects selected", state.selected_node_count());
    } else {
      auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

      if (auto node = state.selected_node(scenes_module.active_scene()); node.is_valid()) {
        _draw_node_properties(state, scenes_module.active_scene(), node, assets_module);
      } else {
        // The selected node no longer exists (e.g. deleted); fall back to the empty state.
        state.clear_selection();
        ImGui::TextDisabled("Nothing selected.");
      }
    }
  } else if (const auto* asset = std::get_if<asset_selection>(&state.current_selection); asset != nullptr) {
    if (asset->kind == asset_kind::prefab) {
      _draw_prefab_edit(state, *asset, assets_module);
    } else {
      _draw_asset_properties(state, *asset, assets_module);
    }
  } else {
    ImGui::TextDisabled("Nothing selected.");
  }

  ImGui::PopStyleVar(2); // FramePadding, ItemSpacing

  ImGui::End();

  ImGui::PopStyleVar(); // WindowPadding
}

} // namespace editor
