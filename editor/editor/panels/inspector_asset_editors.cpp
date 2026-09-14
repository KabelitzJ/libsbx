// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/inspector_panel.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/particle_effect.hpp>

#include <editor/panels/inspector_asset_pickers.hpp>

#include <editor/widgets/text_field.hpp>
#include <editor/widgets/vector_fields.hpp>

namespace editor {

auto inspector_panel::_draw_material_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void {
  if (!_asset_cache.material.is_valid()) {
    ImGui::TextDisabled("Could not load this material.");
    return;
  }

  auto changed = draw_text_field("Name", _material_edit.name);

  static constexpr auto shading_model_names = std::array<const char*, 2u>{"Pbr", "Unlit"};
  auto shading_model_index = static_cast<std::int32_t>(_material_edit.shading);

  if (ImGui::Combo("Shading Model", &shading_model_index, shading_model_names.data(), static_cast<std::int32_t>(shading_model_names.size()))) {
    _material_edit.shading = static_cast<sbx::assets::shading_model>(shading_model_index);
    changed = true;
  }

  const auto is_pbr = _material_edit.shading == sbx::assets::shading_model::pbr;

  changed |= draw_color_field("Base Color", _material_edit.base_color_factor);

  auto emissive = std::array<std::float_t, 3u>{_material_edit.emissive_factor.x(), _material_edit.emissive_factor.y(), _material_edit.emissive_factor.z()};
  if (ImGui::ColorEdit3("Emissive", emissive.data())) {
    _material_edit.emissive_factor = sbx::math::vector3{emissive[0], emissive[1], emissive[2]};
    changed = true;
  }

  // Multiplies emissive_factor unbounded (shaders/lighting.slang) -- the usual way to push a
  // material's own output past bloom_pass's threshold without touching any light in the scene.
  changed |= ImGui::DragFloat("Emissive Strength", &_material_edit.emissive_strength, 0.05f, 0.0f, 100.0f);

  if (is_pbr) {
    changed |= ImGui::DragFloat("Metallic", &_material_edit.metallic_factor, 0.01f, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("Roughness", &_material_edit.roughness_factor, 0.01f, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("IOR", &_material_edit.ior, 0.01f, 1.0f, 3.0f);
    changed |= ImGui::DragFloat("Normal Scale", &_material_edit.normal_scale, 0.01f, 0.0f, 2.0f);
    changed |= ImGui::DragFloat("Occlusion Strength", &_material_edit.occlusion_strength, 0.01f, 0.0f, 1.0f);
  }

  static constexpr auto alpha_mode_names = std::array<const char*, 3u>{"Opaque", "Mask", "Blend"};
  auto alpha_index = static_cast<std::int32_t>(_material_edit.alpha);

  if (ImGui::Combo("Alpha Mode", &alpha_index, alpha_mode_names.data(), static_cast<std::int32_t>(alpha_mode_names.size()))) {
    _material_edit.alpha = static_cast<sbx::assets::alpha_mode>(alpha_index);
    changed = true;
  }

  if (_material_edit.alpha == sbx::assets::alpha_mode::mask) {
    changed |= ImGui::DragFloat("Alpha Cutoff", &_material_edit.alpha_cutoff, 0.01f, 0.0f, 1.0f);
  }

  changed |= ImGui::Checkbox("Double Sided", &_material_edit.is_double_sided);
  changed |= ImGui::Checkbox("Casts Shadow", &_material_edit.casts_shadow);
  changed |= ImGui::Checkbox("Receives Shadow", &_material_edit.receives_shadow);

  auto uv_tiling = std::array<std::float_t, 2u>{_material_edit.uv_tiling.x(), _material_edit.uv_tiling.y()};
  if (draw_vector2_control("UV Tiling", uv_tiling, 1.0f, 0.01f).changed) {
    _material_edit.uv_tiling = sbx::math::vector2{uv_tiling[0], uv_tiling[1]};
    changed = true;
  }

  auto uv_offset = std::array<std::float_t, 2u>{_material_edit.uv_offset.x(), _material_edit.uv_offset.y()};
  if (draw_vector2_control("UV Offset", uv_offset, 0.0f, 0.01f).changed) {
    _material_edit.uv_offset = sbx::math::vector2{uv_offset[0], uv_offset[1]};
    changed = true;
  }

  ImGui::SeparatorText("Textures");

  // A fixed-width label column so every picker button lines up regardless of its label's length
  // ("Metallic/Roughness" vs. "Normal") -- a plain Text+SameLine per row left them staggered.
  if (ImGui::BeginTable("##material_texture_grid", 2, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("##picker", ImGuiTableColumnFlags_WidthStretch);

    const auto texture_row = [&](const char* label, const char* popup_id, sbx::assets::texture_handle& slot, sbx::graphics::format format) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(label);
      ImGui::TableSetColumnIndex(1);
      changed |= draw_texture_picker(state, popup_id, slot, assets_module, format);
    };

    texture_row("Albedo", "##albedo_picker", _material_edit.albedo, sbx::graphics::format::r8g8b8a8_srgb);

    if (is_pbr) {
      texture_row("Normal", "##normal_picker", _material_edit.normal, sbx::graphics::format::r8g8b8a8_unorm);
      texture_row("Metallic/Roughness", "##metallic_roughness_picker", _material_edit.metallic_roughness, sbx::graphics::format::r8g8b8a8_unorm);
      texture_row("Occlusion", "##occlusion_picker", _material_edit.occlusion, sbx::graphics::format::r8g8b8a8_unorm);
    }

    texture_row("Emissive", "##emissive_picker", _material_edit.emissive, sbx::graphics::format::r8g8b8a8_srgb);

    ImGui::EndTable();
  }

  if (changed) {
    // Live preview: mutates in place so every material_handle pointing at it reflects the edit
    // next frame. Disk persistence stays an explicit Save (below).
    assets_module.update_material(_asset_cache.material, _material_edit);
  }

  ImGui::Separator();

  if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
    assets_module.save_material(_asset_cache.material, asset.path);
  }
}

auto inspector_panel::_draw_particle_effect_properties(editor_state& state, const asset_selection& asset, sbx::assets::assets_module& assets_module) -> void {
  if (!_asset_cache.particle_effect.is_valid()) {
    ImGui::TextDisabled("Could not load this particle effect.");
    return;
  }

  auto changed = draw_text_field("Name", _particle_effect_edit.name);

  ImGui::SeparatorText("Emitters");

  static constexpr auto blend_mode_names = std::array<const char*, 2u>{"Additive", "Alpha Blend"};
  static constexpr auto shape_names = std::array<const char*, 4u>{"Point", "Sphere", "Box", "Cone"};
  static constexpr auto simulation_mode_names = std::array<const char*, 2u>{"CPU", "GPU"};

  auto& emitters = _particle_effect_edit.emitters;
  auto removed_index = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < emitters.size(); ++index) {
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& emitter = emitters[index];

    const auto header_label = emitter.name.empty() ? fmt::format("Emitter {}", index) : emitter.name;
    const auto is_expanded = ImGui::TreeNodeEx("##emitter_node", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "%s", header_label.c_str());

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ICON_MDI_DELETE).x - ImGui::GetStyle().FramePadding.x);

    if (ImGui::SmallButton(ICON_MDI_DELETE)) {
      removed_index = index;
      changed = true;
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Remove this emitter");
    }

    if (is_expanded) {
      if (draw_text_field("Name", emitter.name)) {
        changed = true;
      }

      auto blend_mode_index = static_cast<std::int32_t>(emitter.blend_mode);

      if (ImGui::Combo("Blend Mode", &blend_mode_index, blend_mode_names.data(), static_cast<std::int32_t>(blend_mode_names.size()))) {
        emitter.blend_mode = static_cast<sbx::assets::emitter_blend_mode>(blend_mode_index);
        changed = true;
      }

      auto simulation_mode_index = static_cast<std::int32_t>(emitter.simulation_mode);

      if (ImGui::Combo("Simulation Mode", &simulation_mode_index, simulation_mode_names.data(), static_cast<std::int32_t>(simulation_mode_names.size()))) {
        emitter.simulation_mode = static_cast<sbx::assets::particle_simulation_mode>(simulation_mode_index);
        changed = true;
      }

      // GPU silently ignores unsupported emitter configs (billboard-only, no
      // collision/sub-emitters/trails/cone) rather than rejecting them; warn inline.
      if (emitter.simulation_mode == sbx::assets::particle_simulation_mode::gpu && !emitter.supports_gpu_simulation()) {
        ImGui::TextColored(ImVec4{1.0f, 0.7f, 0.2f, 1.0f}, ICON_MDI_ALERT " GPU doesn't support this emitter's current config");

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("GPU is billboard-only, has no collision, sub-emitters, trails, or cone shape support. Unsupported settings are silently ignored while this mode is selected.");
        }
      }

      changed |= ImGui::DragFloat("Emission Rate", &emitter.emission_rate, 0.5f, 0.0f, 10000.0f);

      auto burst_count = static_cast<std::int32_t>(emitter.burst_count);
      if (ImGui::DragInt("Burst Count", &burst_count, 1.0f, 0, 100000)) {
        emitter.burst_count = static_cast<std::uint32_t>(std::max(burst_count, 0));
        changed = true;
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Spawned once, on top of Emission Rate, the moment this emitter becomes active.");
      }

      auto shape_index = static_cast<std::int32_t>(emitter.shape);

      if (ImGui::Combo("Shape", &shape_index, shape_names.data(), static_cast<std::int32_t>(shape_names.size()))) {
        emitter.shape = static_cast<sbx::assets::emitter_shape>(shape_index);
        changed = true;
      }

      if (emitter.shape == sbx::assets::emitter_shape::sphere) {
        changed |= ImGui::DragFloat("Radius", &emitter.shape_extents.x(), 0.01f, 0.0f, 1000.0f);
      } else if (emitter.shape == sbx::assets::emitter_shape::box) {
        auto shape_extents = std::array<std::float_t, 3u>{emitter.shape_extents.x(), emitter.shape_extents.y(), emitter.shape_extents.z()};
        if (draw_vector3_control("Half Extents", shape_extents, 0.0f, 0.01f).changed) {
          emitter.shape_extents = sbx::math::vector3{shape_extents[0], shape_extents[1], shape_extents[2]};
          changed = true;
        }
      } else if (emitter.shape == sbx::assets::emitter_shape::cone) {
        auto cone_angle_degrees = emitter.cone.angle.to_degrees().value();
        if (ImGui::DragFloat("Cone Angle", &cone_angle_degrees, 0.1f, 0.1f, 89.0f)) {
          emitter.cone.angle = sbx::math::degree{cone_angle_degrees};
          changed = true;
        }

        changed |= ImGui::DragFloat("Cone Radius", &emitter.cone.radius, 0.01f, 0.001f, 1000.0f);
        changed |= ImGui::SliderFloat("Cone Emit From Volume", &emitter.cone.emit_from_volume, 0.0f, 1.0f);

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("0 = spawn on the base disc (Unity's \"Emit from: Base\"), 1 = fill the whole cone volume.");
        }
      }

      auto velocity_min = std::array<std::float_t, 3u>{emitter.velocity_min.x(), emitter.velocity_min.y(), emitter.velocity_min.z()};
      if (draw_vector3_control("Velocity Min", velocity_min, 0.0f, 0.05f).changed) {
        emitter.velocity_min = sbx::math::vector3{velocity_min[0], velocity_min[1], velocity_min[2]};
        changed = true;
      }

      auto velocity_max = std::array<std::float_t, 3u>{emitter.velocity_max.x(), emitter.velocity_max.y(), emitter.velocity_max.z()};
      if (draw_vector3_control("Velocity Max", velocity_max, 0.0f, 0.05f).changed) {
        emitter.velocity_max = sbx::math::vector3{velocity_max[0], velocity_max[1], velocity_max[2]};
        changed = true;
      }

      changed |= ImGui::DragFloat("Lifetime Min", &emitter.lifetime_min, 0.01f, 0.0f, 3600.0f);
      changed |= ImGui::DragFloat("Lifetime Max", &emitter.lifetime_max, 0.01f, emitter.lifetime_min, 3600.0f);

      changed |= draw_color_field("Start Color", emitter.start_color);
      changed |= draw_color_field("End Color", emitter.end_color);
      changed |= draw_gradient_editor("Color Over Lifetime (overrides Start/End Color if it has keys)", emitter.color_over_lifetime);

      changed |= ImGui::DragFloat("Size Min", &emitter.size_min, 0.005f, 0.0f, 1000.0f);
      changed |= ImGui::DragFloat("Size Max", &emitter.size_max, 0.005f, emitter.size_min, 1000.0f);
      changed |= draw_curve_editor("Size Over Lifetime (multiplier)", emitter.size_over_lifetime, 0.0f, 10.0f);

      changed |= ImGui::DragFloat("Rotation Min", &emitter.rotation_min, 0.01f, -6.2832f, 6.2832f);
      changed |= ImGui::DragFloat("Rotation Max", &emitter.rotation_max, 0.01f, emitter.rotation_min, 6.2832f);
      changed |= draw_curve_editor("Rotation Over Lifetime (rad/s)", emitter.rotation_over_lifetime, -20.0f, 20.0f);

      changed |= ImGui::DragFloat("Gravity", &emitter.gravity, 0.05f, -1000.0f, 1000.0f);
      changed |= ImGui::DragFloat("Drag", &emitter.drag, 0.01f, 0.0f, 100.0f);

      if (ImGui::TreeNodeEx("Velocity Over Lifetime (m/s, added on top of Velocity Min/Max)", ImGuiTreeNodeFlags_Framed)) {
        changed |= draw_curve_editor("X", emitter.velocity_over_lifetime.x, -100.0f, 100.0f);
        changed |= draw_curve_editor("Y", emitter.velocity_over_lifetime.y, -100.0f, 100.0f);
        changed |= draw_curve_editor("Z", emitter.velocity_over_lifetime.z, -100.0f, 100.0f);
        ImGui::TreePop();
      }

      auto force_min = std::array<std::float_t, 3u>{emitter.force_over_lifetime_min.x(), emitter.force_over_lifetime_min.y(), emitter.force_over_lifetime_min.z()};
      if (draw_vector3_control("Force Min", force_min, 0.0f, 0.05f).changed) {
        emitter.force_over_lifetime_min = sbx::math::vector3{force_min[0], force_min[1], force_min[2]};
        changed = true;
      }

      auto force_max = std::array<std::float_t, 3u>{emitter.force_over_lifetime_max.x(), emitter.force_over_lifetime_max.y(), emitter.force_over_lifetime_max.z()};
      if (draw_vector3_control("Force Max", force_max, 0.0f, 0.05f).changed) {
        emitter.force_over_lifetime_max = sbx::math::vector3{force_max[0], force_max[1], force_max[2]};
        changed = true;
      }

      ImGui::SeparatorText("Rendering");

      static constexpr auto render_mode_names = std::array<const char*, 2u>{"Billboard", "Mesh"};
      auto render_mode_index = static_cast<std::int32_t>(emitter.render_mode);

      if (ImGui::Combo("Render Mode", &render_mode_index, render_mode_names.data(), static_cast<std::int32_t>(render_mode_names.size()))) {
        emitter.render_mode = static_cast<sbx::assets::particle_render_mode>(render_mode_index);
        changed = true;
      }

      if (emitter.render_mode == sbx::assets::particle_render_mode::billboard) {
        ImGui::Text("Texture");
        ImGui::SameLine();

        changed |= draw_texture_picker(state, "##particle_texture_picker", emitter.texture, assets_module, sbx::graphics::format::r8g8b8a8_srgb);
      } else {
        ImGui::Text("Mesh");
        ImGui::SameLine();

        changed |= draw_mesh_picker(state, "##particle_render_mesh_picker", emitter.render_mesh, assets_module);

        ImGui::Text("Material Override");
        ImGui::SameLine();

        changed |= draw_material_picker(state, "##particle_render_material_picker", emitter.render_material, assets_module);

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Leave unset to use each submesh's own authored material.");
        }
      }

      ImGui::SeparatorText("Collision");

      static constexpr auto collision_mode_names = std::array<const char*, 3u>{"None", "Planes", "World"};
      auto collision_mode_index = static_cast<std::int32_t>(emitter.collision.mode);

      if (ImGui::Combo("Collision Mode", &collision_mode_index, collision_mode_names.data(), static_cast<std::int32_t>(collision_mode_names.size()))) {
        emitter.collision.mode = static_cast<sbx::assets::particle_collision_mode>(collision_mode_index);
        changed = true;
      }

      if (emitter.collision.mode != sbx::assets::particle_collision_mode::none) {
        changed |= ImGui::DragFloat("Bounce", &emitter.collision.bounce, 0.01f, 0.0f, 2.0f);
        changed |= ImGui::SliderFloat("Lifetime Loss", &emitter.collision.lifetime_loss, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Dampen", &emitter.collision.dampen, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Radius Scale", &emitter.collision.radius_scale, 0.01f, 0.01f, 10.0f);

        auto max_collisions = static_cast<std::int32_t>(emitter.collision.max_collisions_per_particle);
        if (ImGui::DragInt("Max Collisions", &max_collisions, 1.0f, 0, 100)) {
          emitter.collision.max_collisions_per_particle = static_cast<std::uint32_t>(std::max(max_collisions, 0));
          changed = true;
        }

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("0 = unlimited");
        }
      }

      if (emitter.collision.mode == sbx::assets::particle_collision_mode::planes) {
        auto& planes = emitter.collision.planes;

        ImGui::Text("Planes (%zu / %zu)", planes.size(), sbx::assets::collision_max_planes);

        auto removed_plane_index = std::optional<std::size_t>{};

        for (auto plane_index = std::size_t{0u}; plane_index < planes.size(); ++plane_index) {
          ImGui::PushID(static_cast<std::int32_t>(plane_index));

          auto& plane = planes[plane_index];

          auto normal = std::array<std::float_t, 3u>{plane.normal.x(), plane.normal.y(), plane.normal.z()};
          if (draw_vector3_control("Normal", normal, 0.0f, 0.01f).changed) {
            plane.normal = sbx::math::vector3::normalized(sbx::math::vector3{normal[0], normal[1], normal[2]});
            changed = true;
          }

          changed |= ImGui::DragFloat("Distance", &plane.distance, 0.05f, -1000.0f, 1000.0f);

          if (ImGui::SmallButton(ICON_MDI_DELETE " Remove Plane")) {
            removed_plane_index = plane_index;
            changed = true;
          }

          ImGui::PopID();
        }

        if (removed_plane_index) {
          for (auto i = *removed_plane_index; i + 1u < planes.size(); ++i) {
            planes[i] = planes[i + 1u];
          }

          planes.pop_back();
        }

        ImGui::BeginDisabled(planes.size() >= sbx::assets::collision_max_planes);

        if (ImGui::Button(ICON_MDI_PLUS " Add Plane")) {
          planes.push_back(sbx::assets::collision_plane{});
          changed = true;
        }

        ImGui::EndDisabled();
      }

      ImGui::SeparatorText("Sub-Emitters");

      static constexpr auto sub_emitter_event_names = std::array<const char*, 3u>{"Birth", "Death", "Collision"};

      auto& sub_emitters = emitter.sub_emitters;
      auto removed_sub_emitter_index = std::optional<std::size_t>{};

      for (auto sub_emitter_index = std::size_t{0u}; sub_emitter_index < sub_emitters.size(); ++sub_emitter_index) {
        ImGui::PushID(static_cast<std::int32_t>(sub_emitter_index));

        auto& binding = sub_emitters[sub_emitter_index];

        auto event_index = static_cast<std::int32_t>(binding.event);

        if (ImGui::Combo("Event", &event_index, sub_emitter_event_names.data(), static_cast<std::int32_t>(sub_emitter_event_names.size()))) {
          binding.event = static_cast<sbx::assets::sub_emitter_event>(event_index);
          changed = true;
        }

        ImGui::Text("Effect");
        ImGui::SameLine();

        changed |= draw_particle_effect_picker(state, "##sub_emitter_effect_picker", binding.effect, assets_module);

        changed |= ImGui::SliderFloat("Probability", &binding.probability, 0.0f, 1.0f);
        changed |= ImGui::Checkbox("Inherit Velocity", &binding.inherit_velocity);

        if (ImGui::SmallButton(ICON_MDI_DELETE " Remove Sub-Emitter")) {
          removed_sub_emitter_index = sub_emitter_index;
          changed = true;
        }

        ImGui::Separator();
        ImGui::PopID();
      }

      if (removed_sub_emitter_index) {
        sub_emitters.erase(sub_emitters.begin() + static_cast<std::ptrdiff_t>(*removed_sub_emitter_index));
      }

      if (ImGui::Button(ICON_MDI_PLUS " Add Sub-Emitter")) {
        sub_emitters.push_back(sbx::assets::sub_emitter_binding{});
        changed = true;
      }

      ImGui::SeparatorText("Trail");

      changed |= ImGui::Checkbox("Enabled", &emitter.trail.enabled);

      if (emitter.trail.enabled) {
        changed |= ImGui::DragFloat("Min Vertex Distance", &emitter.trail.min_vertex_distance, 0.005f, 0.001f, 100.0f);
        changed |= ImGui::DragFloat("Trail Lifetime", &emitter.trail.lifetime, 0.01f, 0.01f, 3600.0f);
        changed |= ImGui::DragFloat("Trail Width", &emitter.trail.width, 0.005f, 0.001f, 1000.0f);
        changed |= draw_gradient_editor("Color Over Trail (0 = head, 1 = tail; overrides the particle's own color if it has keys)", emitter.trail.color_over_trail);
        changed |= ImGui::Checkbox("Die With Particle", &emitter.trail.die_with_particle);

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Off (Unity's default): the trail lingers and fades after its particle dies. On: it vanishes immediately.");
        }
      }

      ImGui::TreePop();
    }

    ImGui::PopID();
  }

  if (removed_index) {
    emitters.erase(emitters.begin() + static_cast<std::ptrdiff_t>(*removed_index));
  }

  if (ImGui::Button(ICON_MDI_PLUS " Add Emitter")) {
    emitters.push_back(sbx::assets::particle_emitter{.name = fmt::format("Emitter {}", emitters.size())});
    changed = true;
  }

  if (changed) {
    // Live preview, same as _draw_material_properties — mutates in place; disk persistence stays
    // an explicit Save (below).
    assets_module.update_particle_effect(_asset_cache.particle_effect, _particle_effect_edit);
  }

  ImGui::Separator();

  if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
    assets_module.save_particle_effect(_asset_cache.particle_effect, asset.path);
  }
}

} // namespace editor
