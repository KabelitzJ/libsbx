// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/inspector_panel.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <variant>
#include <vector>

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
#include <editor/commands/prefab_override.hpp>
#include <editor/commands/script_commands.hpp>

#include <editor/panels/inspector_component_registry.hpp>
#include <editor/panels/inspector_script_section.hpp>

#include <editor/widgets/prefab_override_menu.hpp>
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
    // Committed below once editing finishes.
  }

  if (ImGui::IsItemActivated() && !_pending_name_before) {
    _pending_name_before = node.name();
  }

  if (ImGui::IsItemDeactivatedAfterEdit()) {
    // scene::find(name) may go stale after a rename; selection and hierarchy key on ids, not names.
    node.name() = sbx::scenes::tag{std::string{_name_buffer.data()}};

    if (_pending_name_before) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::tag>>(id, *_pending_name_before, node.name(), "Rename Node"));
    }
  }

  if (ImGui::IsItemDeactivated()) {
    _pending_name_before.reset();
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

  const auto transform_override = find_value_override(target, node, component_key<sbx::scenes::local_transform>());
  draw_override_header_marker(transform_override);

  auto& transform = node.transform();

  static auto clipboard = std::optional<sbx::scenes::local_transform>{};

  if (ImGui::BeginPopupContextItem("##transform_context")) {
    const auto push_transform = [&](const sbx::scenes::local_transform& after, const char* label) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::local_transform>>(node.id(), transform, after, label));
    };

    if (ImGui::MenuItem(ICON_MDI_RESTORE " Reset")) {
      push_transform(sbx::scenes::local_transform{}, "Reset Transform");
    }

    if (ImGui::MenuItem("Reset Position")) {
      auto after = transform;
      after.position = sbx::math::vector3{0.0f, 0.0f, 0.0f};
      push_transform(after, "Reset Position");
    }

    if (ImGui::MenuItem("Reset Rotation")) {
      auto after = transform;
      after.rotation = sbx::math::quaternion::identity;
      push_transform(after, "Reset Rotation");
    }

    if (ImGui::MenuItem("Reset Scale")) {
      auto after = transform;
      after.scale = sbx::math::vector3{1.0f, 1.0f, 1.0f};
      push_transform(after, "Reset Scale");
    }

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_MDI_CONTENT_COPY " Copy Values")) {
      clipboard = transform;
    }

    if (ImGui::MenuItem(ICON_MDI_CONTENT_PASTE " Paste Values", nullptr, false, clipboard.has_value())) {
      push_transform(*clipboard, "Paste Transform");
    }

    draw_override_menu_items(target, node, component_key<sbx::scenes::local_transform>(), transform_override);

    ImGui::EndPopup();
  }

  // started snapshots before this frame's change is applied (Reset can start and finish in one frame); committed pushes the command.
  const auto capture_before = [&](const vector3_edit_result& result) {
    if (result.started && !_pending_transform_before) {
      _pending_transform_before = transform;
    }
  };

  const auto commit_after = [&](const vector3_edit_result& result) {
    if (result.committed && _pending_transform_before) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::local_transform>>(node.id(), *_pending_transform_before, transform, "Edit Transform"));
    }

    if (result.ended) {
      _pending_transform_before.reset();
    }
  };

  auto position = std::array<std::float_t, 3u>{transform.position.x(), transform.position.y(), transform.position.z()};
  const auto position_result = draw_vector3_control("Position", position, 0.0f, 0.05f);

  if (overrides_field(transform_override, "position")) {
    draw_override_marker();
  }

  capture_before(position_result);

  if (position_result.changed) {
    transform.position = sbx::math::vector3{position[0], position[1], position[2]};
  }

  commit_after(position_result);

  // Euler angles are cached; see _rotation_node_id.
  if (node.id().value() != _rotation_node_id.value() || !(transform.rotation == _rotation_cache)) {
    const auto euler = sbx::math::quaternion::euler_angles(transform.rotation);
    _rotation = {euler.x(), euler.y(), euler.z()};
    _rotation_node_id = node.id();
    _rotation_cache = transform.rotation;
  }

  const auto rotation_result = draw_vector3_control("Rotation", _rotation, 0.0f, 0.5f);

  if (overrides_field(transform_override, "rotation")) {
    draw_override_marker();
  }

  capture_before(rotation_result);

  if (rotation_result.changed) {
    transform.rotation = sbx::math::quaternion{sbx::math::vector3{_rotation[0], _rotation[1], _rotation[2]}};
    _rotation_cache = transform.rotation;
  }

  commit_after(rotation_result);

  auto scale = std::array<std::float_t, 3u>{transform.scale.x(), transform.scale.y(), transform.scale.z()};
  const auto scale_result = draw_vector3_control("Scale", scale, 1.0f, 0.05f);

  if (overrides_field(transform_override, "scale")) {
    draw_override_marker();
  }

  capture_before(scale_result);

  if (scale_result.changed) {
    transform.scale = sbx::math::vector3{scale[0], scale[1], scale[2]};
  }

  commit_after(scale_result);
}

auto inspector_panel::_draw_node_properties(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module, bool draw_identity, std::span<const sbx::math::uuid> multi_selection) -> void {
  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  const auto is_multi = !multi_selection.empty();

  // Breathing room between sections.
  const auto section_gap = [] { ImGui::Dummy(ImVec2{0.0f, 6.0f}); };

  if (is_multi) {
    ImGui::TextDisabled("%zu objects selected -- showing %s's values", multi_selection.size(), node.name().c_str());
    _draw_layer_field(state, target, node);
    section_gap();
  } else if (draw_identity) {
    _draw_active_checkbox(state, target, node);
    _draw_name_field(state, target, node);
    _draw_layer_field(state, target, node);
    section_gap();
  }

  if (!is_multi && node.has_component<sbx::scenes::prefab_instance>()) {
    _draw_prefab_instance_header(target, node);
    section_gap();
  }

  if (draw_identity) {
    _draw_transform_section(state, target, node);
  }

  const auto all_selected_have = [&](const component_entry& entry) {
    return std::ranges::all_of(multi_selection, [&](const auto id) {
      const auto selected = target.find(id);
      return selected.is_valid() && entry.has(selected);
    });
  };

  // Driven by component_entries(), which also drives Add Component, so each type is registered once.
  for (const auto& entry : component_entries()) {
    if (entry.has(node) && all_selected_have(entry)) {
      section_gap();
      ImGui::PushID(entry.name);
      entry.draw(state, target, node, assets_module);
      ImGui::PopID();
    }
  }

  if (is_multi) {
    section_gap();
    ImGui::TextDisabled("Scripts and Add Component need a single selection.");
    return;
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
    _shader_graph_seed_pending = false; // the material's own generic_params are already meaningful

    switch (asset.kind) {
      case asset_kind::texture: _asset_cache.texture = assets_module.load_texture(asset.id); break;
      case asset_kind::mesh: _asset_cache.mesh = assets_module.load_mesh(asset.id); break;
      case asset_kind::material: _asset_cache.material = assets_module.load_material(asset.id); break;
      case asset_kind::environment_map: _asset_cache.environment_map = assets_module.load_environment_map(asset.id); break;
      case asset_kind::particle_effect: _asset_cache.particle_effect = assets_module.load_particle_effect(asset.id); break;
      case asset_kind::animation_graph: _asset_cache.animation_graph = assets_module.load_animation_graph(asset.id); break;
      case asset_kind::shader_graph: _asset_cache.shader_graph = assets_module.load_shader_graph(asset.id); break;
      case asset_kind::font: _asset_cache.font = assets_module.load_font(asset.id); break;
      case asset_kind::scene: _asset_cache.scene = assets_module.load_scene(asset.id); break;
      case asset_kind::prefab:
      case asset_kind::script:
      case asset_kind::unknown:
        break;
    }

    if (asset.kind == asset_kind::particle_effect && _asset_cache.particle_effect.is_valid()) {
      const auto& effect = *_asset_cache.particle_effect;

      _particle_effect_edit.name = effect.name();
      _particle_effect_edit.emitters = effect.emitters();
    }

    // Seeded once the asynchronous load lands; copying now could capture placeholder defaults that a Save would write back.
    _material_edit_pending = asset.kind == asset_kind::material;
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

      // A read-only summary; states and transitions are edited in the graph editor.
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
    case asset_kind::shader_graph: {
      ImGui::Text("Type: Shader Graph");

      if (ImGui::Button(ICON_MDI_VECTOR_POLYLINE " Open Graph Editor")) {
        state.request_open_shader_graph_editor(asset.id, asset.path);
      }

      // A read-only summary; nodes and edges are edited in the graph editor.
      const auto& handle = _asset_cache.shader_graph;

      if (handle.is_valid()) {
        ImGui::Text("Nodes: %zu", handle->nodes().size());
        ImGui::Text("Edges: %zu", handle->edges().size());

        const auto parameters = handle->parameters();

        if (!parameters.empty() && ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
          for (const auto& parameter : parameters) {
            const auto* type_name = [&] {
              switch (parameter.type) {
                case sbx::assets::shader_graph_parameter_type::float_value: return "Float";
                case sbx::assets::shader_graph_parameter_type::vector3_value: return "Vector3";
                case sbx::assets::shader_graph_parameter_type::color_value: return "Color";
                case sbx::assets::shader_graph_parameter_type::texture_value: return "Texture";
              }
              return "?";
            }();

            ImGui::Text("%s (%s)", parameter.name.empty() ? "(unnamed)" : parameter.name.c_str(), type_name);
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
      // Unreachable: draw() routes prefabs to _draw_prefab_edit. Kept so the switch stays exhaustive.
      break;
    }
    case asset_kind::scene: {
      ImGui::Text("Type: Scene");

      if (ImGui::Button(ICON_MDI_FOLDER_OPEN " Open")) {
        state.request_open_scene(asset.path);
      }

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
  // WindowPadding must be pushed before Begin().
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{10.0f, 10.0f});

  ImGui::Begin(window_name);

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{6.0f, 4.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{8.0f, 6.0f});

  // Fields fill to a 40% label column instead of ImGui's 65%, so long labels don't clip.
  ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.4f);

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (std::holds_alternative<node_selection>(state.current_selection)) {
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    if (auto node = state.selected_node(scenes_module.active_scene()); node.is_valid() && state.selected_node_count() > 1u) {
      // Copied: the selection may change mid-draw, the broadcast list mustn't.
      const auto selection = std::vector<sbx::math::uuid>{state.selected_node_ids().begin(), state.selected_node_ids().end()};

      state.broadcast_targets = selection;
      _draw_node_properties(state, scenes_module.active_scene(), node, assets_module, true, selection);
      state.broadcast_targets.clear();
    } else {
      if (node.is_valid()) {
        _draw_node_properties(state, scenes_module.active_scene(), node, assets_module);
      } else {
        // The selected node no longer exists.
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

  ImGui::PopItemWidth();
  ImGui::PopStyleVar(2); // FramePadding, ItemSpacing

  ImGui::End();

  ImGui::PopStyleVar(); // WindowPadding
}

} // namespace editor
