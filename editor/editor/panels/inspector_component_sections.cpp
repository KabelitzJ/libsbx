// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/inspector_component_sections.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <type_traits>
#include <variant>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>

#include <libsbx/canvas/components.hpp>

#include <editor/commands/component_commands.hpp>
#include <editor/commands/scene_commands.hpp>

#include <editor/panels/inspector_asset_pickers.hpp>

#include <editor/widgets/vector_fields.hpp>

namespace editor {

using widgets::bracket_edit;
using widgets::draw_color_field;
using widgets::draw_vector2_control;
using widgets::vector2_edit_result;

auto draw_camera_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_CAMERA_OUTLINE " Camera", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::scenes::camera>>(node.id(), node.get_component<sbx::scenes::camera>(), "Remove Camera"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& camera = node.get_component<sbx::scenes::camera>();
  static auto pending = std::optional<sbx::scenes::camera>{};

  ImGui::DragFloat("FOV (degrees)", &camera.fov_degrees, 0.5f, 1.0f, 179.0f);
  bracket_edit(state, target, node, camera, pending, "Edit Camera");
  ImGui::DragFloat("Near Plane", &camera.near_plane, 0.01f, 0.001f, camera.far_plane);
  bracket_edit(state, target, node, camera, pending, "Edit Camera");
  ImGui::DragFloat("Far Plane", &camera.far_plane, 1.0f, camera.near_plane, 100000.0f);
  bracket_edit(state, target, node, camera, pending, "Edit Camera");
  ImGui::DragFloat("Exposure", &camera.exposure, 0.05f, -8.0f, 8.0f);
  bracket_edit(state, target, node, camera, pending, "Edit Camera");
  ImGui::Checkbox("Bloom", &camera.bloom_enabled);
  bracket_edit(state, target, node, camera, pending, "Edit Camera");
  ImGui::DragFloat("Bloom Intensity", &camera.bloom_intensity, 0.005f, 0.0f, 2.0f);
  bracket_edit(state, target, node, camera, pending, "Edit Camera");
  ImGui::DragFloat("Bloom Threshold", &camera.bloom_threshold, 0.02f, 0.0f, 10.0f);
  bracket_edit(state, target, node, camera, pending, "Edit Camera");
  ImGui::DragFloat("Bloom Knee", &camera.bloom_knee, 0.01f, 0.0f, 2.0f);
  bracket_edit(state, target, node, camera, pending, "Edit Camera");

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  const auto is_active_camera = scene.has_active_camera() && scene.active_camera().id() == node.id();

  if (is_active_camera) {
    ImGui::BeginDisabled();
    ImGui::Button("Active Camera");
    ImGui::EndDisabled();
  } else if (ImGui::Button("Set as Active Camera")) {
    state.push_command(target, std::make_unique<set_active_camera_command>(target, node));
  }
}

auto draw_mesh_renderer_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_CUBE_OUTLINE " Mesh Renderer", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::scenes::mesh_renderer>>(node.id(), node.get_component<sbx::scenes::mesh_renderer>(), "Remove Mesh Renderer"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& renderer = node.get_component<sbx::scenes::mesh_renderer>();

  // Resolved within this single frame (popup-based pickers, not a multi-frame drag) — snapshot
  // once up front and push at most one command at the end if anything below actually changed.
  const auto before = renderer;
  auto changed = false;

  const auto previous_mesh_id = renderer.mesh.is_valid() ? renderer.mesh->id() : sbx::math::uuid::nil();

  ImGui::Text("Mesh:");
  ImGui::SameLine();
  changed |= draw_mesh_picker(state, "##mesh_picker_popup", renderer.mesh, assets_module);

  const auto new_mesh_id = renderer.mesh.is_valid() ? renderer.mesh->id() : sbx::math::uuid::nil();

  if (new_mesh_id != previous_mesh_id) {
    // A different mesh was just picked — its own submeshes' materials should take over cleanly,
    // not share slots (by index) with whatever the previous mesh happened to have.
    renderer.materials.clear();
  }

  sbx::scenes::sync_materials_with_mesh(renderer);

  // skeleton_pose is fully auto-managed, not user-addable/removable (see scenes::skeleton_pose's
  // doc comment) -- its presence tracks whether the assigned mesh actually has a skeleton, exactly
  // like sync_materials_with_mesh backfilling material slots above.
  if (renderer.mesh.is_valid() && renderer.mesh->skeleton().is_valid()) {
    node.get_or_add_component<sbx::scenes::skeleton_pose>().skeleton = renderer.mesh->skeleton();
  } else if (node.has_component<sbx::scenes::skeleton_pose>()) {
    node.remove_component<sbx::scenes::skeleton_pose>();
  }

  if (renderer.mesh.is_valid()) {
    ImGui::Text("Submeshes: %zu", renderer.mesh->submeshes().size());
  } else {
    ImGui::TextDisabled("No mesh assigned.");
  }

  for (auto index = std::size_t{0u}; index < renderer.materials.size(); ++index) {
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& slot = renderer.materials[index];
    const auto mesh_default = (renderer.mesh.is_valid() && index < renderer.mesh->submeshes().size())
      ? renderer.mesh->submeshes()[index].material
      : sbx::assets::material_handle{};

    ImGui::Text("Material %zu:", index);
    ImGui::SameLine();
    changed |= draw_material_picker(state, "##material_picker_popup", slot, assets_module, mesh_default);

    if (slot.is_valid()) {
      ImGui::SameLine();

      if (ImGui::Button(ICON_MDI_EXPORT_VARIANT " Duplicate")) {
        slot = extract_material_to_asset(assets_module, slot, renderer.mesh.is_valid() ? renderer.mesh->id() : sbx::math::uuid::nil());
        changed = true;
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Fork this slot's material into an independent copy, so editing it only affects this node.");
      }
    }

    ImGui::PopID();
  }

  if (changed) {
    state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::mesh_renderer>>(node.id(), before, renderer, "Edit Mesh Renderer"));
  }
}

auto draw_animator_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_ANIMATION_PLAY " Animator", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::scenes::animator>>(node.id(), node.get_component<sbx::scenes::animator>(), "Remove Animator"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& anim = node.get_component<sbx::scenes::animator>();
  static auto pending = std::optional<sbx::scenes::animator>{};

  const auto* mesh = (node.has_component<sbx::scenes::mesh_renderer>() && node.get_component<sbx::scenes::mesh_renderer>().mesh.is_valid())
    ? node.get_component<sbx::scenes::mesh_renderer>().mesh.get()
    : nullptr;

  if (mesh == nullptr || !mesh->skeleton().is_valid()) {
    ImGui::TextDisabled("Assign a skinned mesh (Mesh Renderer) to this node first.");
    return;
  }

  ImGui::Text("Graph:");
  ImGui::SameLine();

  auto graph_handle = anim.graph;

  if (draw_animation_graph_picker(state, "##animation_graph_picker_popup", graph_handle, mesh->id())) {
    const auto before = anim;
    anim.set_graph(graph_handle); // not a plain assignment -- reseeds parameters/current_state_id from the new graph's own defaults
    state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::animator>>(node.id(), before, anim, "Edit Animator"));
  }

  if (!anim.graph.is_valid()) {
    ImGui::TextDisabled("Assign an Animation Graph asset to drive this mesh.");
    return;
  }

  {
    const auto before = anim;

    if (ImGui::Checkbox("Playing", &anim.playing)) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::animator>>(node.id(), before, anim, "Edit Animator"));
    }
  }

  const auto& states = anim.graph->states();

  const auto current_state_it = std::ranges::find(states, anim.current_state_id, &sbx::assets::animation_state::id);
  ImGui::Text("Current State: %s", current_state_it != states.end() ? current_state_it->name.c_str() : "(none)");

  // Always drawn (a progress bar rather than text that only appears mid-transition) so this row's
  // height never changes as transitions start/stop -- nothing below it should ever jump.
  const auto is_transitioning = anim.transition_target_state_id.has_value();
  auto overlay = std::string{"Not transitioning"};
  auto alpha = 0.0f;

  if (is_transitioning) {
    const auto target_state_it = std::ranges::find(states, *anim.transition_target_state_id, &sbx::assets::animation_state::id);
    alpha = (anim.transition_duration > 0.0f) ? std::clamp(anim.transition_time / anim.transition_duration, 0.0f, 1.0f) : 1.0f;
    overlay = fmt::format("-> {} ({:.0f}%)", target_state_it != states.end() ? target_state_it->name.c_str() : "(none)", alpha * 100.0f);
  }

  ImGui::ProgressBar(alpha, ImVec2{-1.0f, 0.0f}, overlay.c_str());

  // States/transitions themselves are hand-authored in the .animation_graph file until the visual
  // graph editor lands (see plan) -- this section is only for testing them: assigning parameter
  // values live, the same way gameplay code would via animator::set_float/set_bool/set_trigger.
  if (!anim.parameters.empty() && ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
    for (auto& [name, value] : anim.parameters) {
      ImGui::PushID(name.c_str());

      std::visit([&](auto& current) {
        using value_type = std::decay_t<decltype(current)>;

        if constexpr (std::is_same_v<value_type, std::float_t>) {
          ImGui::DragFloat(name.c_str(), &current, 0.05f);
          bracket_edit(state, target, node, anim, pending, "Edit Animator");
        } else if constexpr (std::is_same_v<value_type, bool>) {
          const auto before = anim;

          if (ImGui::Checkbox(name.c_str(), &current)) {
            state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::animator>>(node.id(), before, anim, "Edit Animator"));
          }
        } else if constexpr (std::is_same_v<value_type, std::int32_t>) {
          ImGui::DragInt(name.c_str(), &current);
          bracket_edit(state, target, node, anim, pending, "Edit Animator");
        } else {
          ImGui::AlignTextToFramePadding();
          ImGui::TextUnformatted(name.c_str());
          ImGui::SameLine();

          if (ImGui::Button("Fire")) {
            const auto before = anim;
            current.set = true;
            state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::animator>>(node.id(), before, anim, "Edit Animator"));
          }
        }
      }, value);

      ImGui::PopID();
    }

    ImGui::TreePop();
  }
}

auto draw_directional_light_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_WHITE_BALANCE_SUNNY " Directional Light", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::scenes::directional_light>>(node.id(), node.get_component<sbx::scenes::directional_light>(), "Remove Directional Light"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::directional_light>();
  static auto pending = std::optional<sbx::scenes::directional_light>{};

  draw_color_field("Color", light.color);
  bracket_edit(state, target, node, light, pending, "Edit Directional Light");
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
  bracket_edit(state, target, node, light, pending, "Edit Directional Light");

  {
    const auto before = light;
    if (ImGui::Checkbox("Casts Shadows", &light.casts_shadows)) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::directional_light>>(node.id(), before, light, "Edit Directional Light"));
    }
  }

  ImGui::DragFloat("Shadow Distance", &light.shadow_distance, 0.5f, 1.0f, 1000.0f);
  bracket_edit(state, target, node, light, pending, "Edit Directional Light");
}

auto draw_point_light_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_LIGHTBULB_OUTLINE " Point Light", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::scenes::point_light>>(node.id(), node.get_component<sbx::scenes::point_light>(), "Remove Point Light"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::point_light>();
  static auto pending = std::optional<sbx::scenes::point_light>{};

  draw_color_field("Color", light.color);
  bracket_edit(state, target, node, light, pending, "Edit Point Light");
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
  bracket_edit(state, target, node, light, pending, "Edit Point Light");
  ImGui::DragFloat("Range", &light.range, 0.05f, 0.0f, 10000.0f);
  bracket_edit(state, target, node, light, pending, "Edit Point Light");
}

auto draw_spot_light_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_FLASHLIGHT " Spot Light", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::scenes::spot_light>>(node.id(), node.get_component<sbx::scenes::spot_light>(), "Remove Spot Light"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& light = node.get_component<sbx::scenes::spot_light>();
  static auto pending = std::optional<sbx::scenes::spot_light>{};

  draw_color_field("Color", light.color);
  bracket_edit(state, target, node, light, pending, "Edit Spot Light");
  ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f);
  bracket_edit(state, target, node, light, pending, "Edit Spot Light");
  ImGui::DragFloat("Range", &light.range, 0.05f, 0.0f, 10000.0f);
  bracket_edit(state, target, node, light, pending, "Edit Spot Light");

  // inner_angle/outer_angle are stored in radians; SliderAngle operates on a radians pointer
  // while displaying/editing in degrees, so these bind directly — no manual conversion.
  ImGui::SliderAngle("Inner Angle", &light.inner_angle, 0.0f, 90.0f);
  bracket_edit(state, target, node, light, pending, "Edit Spot Light");
  ImGui::SliderAngle("Outer Angle", &light.outer_angle, 0.0f, 90.0f);
  bracket_edit(state, target, node, light, pending, "Edit Spot Light");
}

auto draw_skybox_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_EARTH " Skybox", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::scenes::skybox>>(node.id(), node.get_component<sbx::scenes::skybox>(), "Remove Skybox"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& sky = node.get_component<sbx::scenes::skybox>();
  static auto pending = std::optional<sbx::scenes::skybox>{};

  if (sky.environment.is_valid()) {
    ImGui::Text("Environment: %s", widgets::asset_path_text(assets_module, sky.environment->id()).c_str());

    const auto* environment = sky.environment.get();
    const auto is_baked = environment->radiance_index() != sbx::assets::environment_map::invalid_index && environment->irradiance_index() != sbx::assets::environment_map::invalid_index && environment->prefiltered_index() != sbx::assets::environment_map::invalid_index;
    ImGui::Text("Baked: %s", is_baked ? "yes" : "no");
  } else {
    ImGui::TextDisabled("Environment: (none)");
  }

  ImGui::DragFloat("Background Intensity", &sky.intensity, 0.05f, 0.0f, 100.0f);
  bracket_edit(state, target, node, sky, pending, "Edit Skybox");
  ImGui::DragFloat("Ambient Intensity", &sky.ambient_intensity, 0.05f, 0.0f, 100.0f);
  bracket_edit(state, target, node, sky, pending, "Edit Skybox");
}

auto draw_particle_effect_instance_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_FIREWORK " Particle Effect", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::scenes::particle_effect>>(node.id(), node.get_component<sbx::scenes::particle_effect>(), "Remove Particle Effect"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& instance = node.get_component<sbx::scenes::particle_effect>();

  ImGui::Text("Effect:");
  ImGui::SameLine();

  {
    const auto before = instance;

    if (draw_particle_effect_picker(state, "##particle_effect_picker_popup", instance.effect, assets_module)) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::particle_effect>>(node.id(), before, instance, "Edit Particle Effect"));
    }
  }

  if (instance.effect.is_valid()) {
    ImGui::Text("Emitters: %zu", instance.effect->emitters().size());
  } else {
    ImGui::TextDisabled("No effect assigned.");
  }

  {
    const auto before = instance;

    if (ImGui::Checkbox("Loop", &instance.loop)) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::particle_effect>>(node.id(), before, instance, "Edit Particle Effect"));
    }

    ImGui::BeginDisabled(instance.loop);

    if (ImGui::DragFloat("Duration", &instance.duration, 0.05f, 0.01f, 3600.0f)) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::particle_effect>>(node.id(), before, instance, "Edit Particle Effect"));
    }

    ImGui::EndDisabled();
  }

  ImGui::SeparatorText("Playback");

  // Play/Pause/Stop below are transport controls, not authored data — deliberately excluded from
  // undo/redo (only Loop above is tracked).

  const auto is_playing = instance.playback == sbx::scenes::particle_playback_state::playing;
  const auto is_stopped = instance.playback == sbx::scenes::particle_playback_state::stopped;

  ImGui::BeginDisabled(!instance.effect.is_valid());

  if (is_playing) {
    if (ImGui::Button(ICON_MDI_PAUSE " Pause")) {
      instance.playback = sbx::scenes::particle_playback_state::paused;
    }
  } else if (ImGui::Button(ICON_MDI_PLAY " Play")) {
    instance.playback = sbx::scenes::particle_playback_state::playing;
  }

  ImGui::EndDisabled();

  ImGui::SameLine();

  ImGui::BeginDisabled(is_stopped);

  if (ImGui::Button(ICON_MDI_STOP " Stop")) {
    // Only flips playback -- particles_module::fixed_update() notices next frame and clears every
    // emitter's particle array on its own.
    instance.playback = sbx::scenes::particle_playback_state::stopped;
    instance.elapsed = 0.0f;
  }

  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::TextDisabled("(%s)", is_playing ? "Playing" : is_stopped ? "Stopped" : "Paused");
}

auto draw_rigidbody_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_SOCCER " Rigidbody", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::physics::rigidbody>>(node.id(), node.get_component<sbx::physics::rigidbody>(), "Remove Rigidbody"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& body = node.get_component<sbx::physics::rigidbody>();
  static auto pending = std::optional<sbx::physics::rigidbody>{};

  static constexpr auto body_type_names = std::array<const char*, 3u>{"Dynamic", "Kinematic", "Static"};
  const auto current_index = static_cast<std::size_t>(body.type);

  if (ImGui::BeginCombo("Body Type", body_type_names[current_index])) {
    for (auto index = std::size_t{0u}; index < body_type_names.size(); ++index) {
      const auto is_selected = (index == current_index);

      if (ImGui::Selectable(body_type_names[index], is_selected) && index != current_index) {
        const auto before = body;
        body.type = static_cast<sbx::physics::body_type>(index);
        state.push_command(target, std::make_unique<modify_component_command<sbx::physics::rigidbody>>(node.id(), before, body, "Edit Rigidbody"));
      }

      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }

  // inverse_mass is the stored source of truth (see rigidbody's doc comment) — the field here
  // just presents/edits its reciprocal.
  auto mass = (body.inverse_mass > 0.0f) ? (1.0f / body.inverse_mass) : 0.0f;

  if (ImGui::DragFloat("Mass (kg)", &mass, 0.05f, 0.0f, 100000.0f)) {
    body.inverse_mass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
  }

  bracket_edit(state, target, node, body, pending, "Edit Rigidbody");

  ImGui::DragFloat("Linear Damping", &body.linear_damping, 0.005f, 0.0f, 10.0f);
  bracket_edit(state, target, node, body, pending, "Edit Rigidbody");
  ImGui::DragFloat("Angular Damping", &body.angular_damping, 0.005f, 0.0f, 10.0f);
  bracket_edit(state, target, node, body, pending, "Edit Rigidbody");
  ImGui::DragFloat("Gravity Scale", &body.gravity_scale, 0.05f, -10.0f, 10.0f);
  bracket_edit(state, target, node, body, pending, "Edit Rigidbody");

  auto linear_velocity = std::array<std::float_t, 3u>{body.linear_velocity.x(), body.linear_velocity.y(), body.linear_velocity.z()};

  if (ImGui::DragFloat3("Linear Velocity", linear_velocity.data(), 0.05f)) {
    body.linear_velocity = sbx::math::vector3{linear_velocity[0], linear_velocity[1], linear_velocity[2]};
  }

  bracket_edit(state, target, node, body, pending, "Edit Rigidbody");

  auto angular_velocity = std::array<std::float_t, 3u>{body.angular_velocity.x(), body.angular_velocity.y(), body.angular_velocity.z()};

  if (ImGui::DragFloat3("Angular Velocity", angular_velocity.data(), 0.05f)) {
    body.angular_velocity = sbx::math::vector3{angular_velocity[0], angular_velocity[1], angular_velocity[2]};
  }

  bracket_edit(state, target, node, body, pending, "Edit Rigidbody");
}

auto draw_nav_agent_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_WALK " Nav Agent", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::physics::nav_agent>>(node.id(), node.get_component<sbx::physics::nav_agent>(), "Remove Nav Agent"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& agent = node.get_component<sbx::physics::nav_agent>();
  static auto pending = std::optional<sbx::physics::nav_agent>{};

  ImGui::DragFloat("Radius", &agent.radius, 0.01f, 0.01f, 5.0f);
  bracket_edit(state, target, node, agent, pending, "Edit Nav Agent");
  ImGui::DragFloat("Height", &agent.height, 0.01f, 0.01f, 5.0f);
  bracket_edit(state, target, node, agent, pending, "Edit Nav Agent");
  ImGui::DragFloat("Base Offset", &agent.base_offset, 0.01f, -5.0f, 5.0f);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("How far above the navmesh surface this node's pivot sits — half the height for a capsule centered on its own origin, 0 for a foot-pivoted model.");
  }
  bracket_edit(state, target, node, agent, pending, "Edit Nav Agent");
  ImGui::DragFloat("Max Speed", &agent.max_speed, 0.05f, 0.0f, 50.0f);
  bracket_edit(state, target, node, agent, pending, "Edit Nav Agent");
  ImGui::DragFloat("Max Acceleration", &agent.max_acceleration, 0.05f, 0.0f, 100.0f);
  bracket_edit(state, target, node, agent, pending, "Edit Nav Agent");
  ImGui::DragFloat("Collision Query Range", &agent.collision_query_range, 0.05f, 0.0f, 50.0f);
  bracket_edit(state, target, node, agent, pending, "Edit Nav Agent");
  ImGui::DragFloat("Path Optimization Range", &agent.path_optimization_range, 0.05f, 0.0f, 50.0f);
  bracket_edit(state, target, node, agent, pending, "Edit Nav Agent");
  ImGui::DragFloat("Separation Weight", &agent.separation_weight, 0.05f, 0.0f, 20.0f);
  bracket_edit(state, target, node, agent, pending, "Edit Nav Agent");

  {
    const auto before = agent;

    if (ImGui::Checkbox("Obstacle Avoidance", &agent.obstacle_avoidance_enabled)) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::physics::nav_agent>>(node.id(), before, agent, "Edit Nav Agent"));
    }
  }

  ImGui::SameLine();

  {
    const auto before = agent;

    if (ImGui::Checkbox("Separation", &agent.separation_enabled)) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::physics::nav_agent>>(node.id(), before, agent, "Edit Nav Agent"));
    }
  }

  ImGui::Separator();

  static constexpr auto state_names = std::array<const char*, 3u>{"Idle", "Moving", "Target Unreachable"};

  ImGui::TextDisabled("State: %s", state_names[static_cast<std::size_t>(agent.state)]);
  ImGui::TextDisabled("Velocity: %.2f, %.2f, %.2f", agent.velocity.x(), agent.velocity.y(), agent.velocity.z());
  ImGui::TextDisabled("Target: %.2f, %.2f, %.2f", agent.target.x(), agent.target.y(), agent.target.z());
}

// offset/rotation are shared by shape_collider and mesh_collider — same fields, same widgets.
template<typename Collider>
auto draw_collider_offset_rotation_friction(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, Collider& collider, std::optional<Collider>& pending, const char* label) -> void {
  auto offset = std::array<std::float_t, 3u>{collider.offset.x(), collider.offset.y(), collider.offset.z()};

  if (ImGui::DragFloat3("Offset", offset.data(), 0.05f)) {
    collider.offset = sbx::math::vector3{offset[0], offset[1], offset[2]};
  }

  bracket_edit(state, target, node, collider, pending, label);

  const auto euler = sbx::math::quaternion::euler_angles(collider.rotation);
  auto rotation_degrees = std::array<std::float_t, 3u>{euler.x(), euler.y(), euler.z()};

  if (ImGui::DragFloat3("Rotation", rotation_degrees.data(), 0.5f)) {
    collider.rotation = sbx::math::quaternion{sbx::math::vector3{rotation_degrees[0], rotation_degrees[1], rotation_degrees[2]}};
  }

  bracket_edit(state, target, node, collider, pending, label);

  ImGui::DragFloat("Friction", &collider.friction, 0.01f, 0.0f, 10.0f);
  bracket_edit(state, target, node, collider, pending, label);
  ImGui::DragFloat("Restitution", &collider.restitution, 0.01f, 0.0f, 1.0f);
  bracket_edit(state, target, node, collider, pending, label);

  {
    const auto before = collider;

    if (ImGui::Checkbox("Is Trigger", &collider.is_trigger)) {
      state.push_command(target, std::make_unique<modify_component_command<Collider>>(node.id(), before, collider, label));
    }
  }

  if (collider.is_trigger) {
    ImGui::TextDisabled("Detected (physics_module::on_contact_began/on_contact_ended) but never physically pushes anything.");
  }
}

auto draw_shape_collider_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_SHAPE_OUTLINE " Shape Collider", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::physics::shape_collider>>(node.id(), node.get_component<sbx::physics::shape_collider>(), "Remove Shape Collider"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& collider = node.get_component<sbx::physics::shape_collider>();
  static auto pending = std::optional<sbx::physics::shape_collider>{};

  if (node.has_component<sbx::physics::mesh_collider>()) {
    // Narrowphase only resolves one collider per node (shape_collider wins; see
    // narrowphase.cpp's resolve_convex) — Add Component already blocks creating both.
    ImGui::TextColored(ImVec4{1.0f, 0.7f, 0.2f, 1.0f}, ICON_MDI_ALERT_OUTLINE " Also has a Mesh Collider -- it will be ignored.");
  }

  static constexpr auto shape_names = std::array<const char*, 4u>{"Sphere", "Cylinder", "Capsule", "Box"};
  const auto current_index = std::min(collider.shape.index(), std::size_t{3u}); // clamp: index 4 (triangle) never legitimately appears here

  if (ImGui::BeginCombo("Shape", shape_names[current_index])) {
    for (auto index = std::size_t{0u}; index < shape_names.size(); ++index) {
      const auto is_selected = (index == current_index);

      if (ImGui::Selectable(shape_names[index], is_selected) && index != current_index) {
        const auto before = collider;

        switch (index) {
          case 0u: collider.shape = sbx::physics::sphere{}; break;
          case 1u: collider.shape = sbx::physics::cylinder{}; break;
          case 2u: collider.shape = sbx::physics::capsule{}; break;
          case 3u: collider.shape = sbx::physics::box{}; break;
          default: break;
        }

        state.push_command(target, std::make_unique<modify_component_command<sbx::physics::shape_collider>>(node.id(), before, collider, "Change Collider Shape"));
      }

      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }

  if (auto* sphere = std::get_if<sbx::physics::sphere>(&collider.shape)) {
    ImGui::DragFloat("Radius", &sphere->radius, 0.05f, 0.001f, 1000.0f);
    bracket_edit(state, target, node, collider, pending, "Edit Shape Collider");
  } else if (auto* cylinder = std::get_if<sbx::physics::cylinder>(&collider.shape)) {
    ImGui::DragFloat("Radius", &cylinder->radius, 0.05f, 0.001f, 1000.0f);
    bracket_edit(state, target, node, collider, pending, "Edit Shape Collider");
    ImGui::DragFloat("Half Height", &cylinder->half_height, 0.05f, 0.001f, 1000.0f);
    bracket_edit(state, target, node, collider, pending, "Edit Shape Collider");
  } else if (auto* capsule = std::get_if<sbx::physics::capsule>(&collider.shape)) {
    ImGui::DragFloat("Radius", &capsule->radius, 0.05f, 0.001f, 1000.0f);
    bracket_edit(state, target, node, collider, pending, "Edit Shape Collider");
    ImGui::DragFloat("Half Height", &capsule->half_height, 0.05f, 0.001f, 1000.0f);
    bracket_edit(state, target, node, collider, pending, "Edit Shape Collider");
  } else if (auto* box = std::get_if<sbx::physics::box>(&collider.shape)) {
    auto half_extents = std::array<std::float_t, 3u>{box->half_extents.x(), box->half_extents.y(), box->half_extents.z()};

    if (ImGui::DragFloat3("Half Extents", half_extents.data(), 0.05f, 0.001f, 1000.0f)) {
      box->half_extents = sbx::math::vector3{half_extents[0], half_extents[1], half_extents[2]};
    }

    bracket_edit(state, target, node, collider, pending, "Edit Shape Collider");
  }

  draw_collider_offset_rotation_friction(state, target, node, collider, pending, "Edit Shape Collider");
}

auto draw_mesh_collider_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_TERRAIN " Mesh Collider", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::physics::mesh_collider>>(node.id(), node.get_component<sbx::physics::mesh_collider>(), "Remove Mesh Collider"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& collider = node.get_component<sbx::physics::mesh_collider>();
  static auto pending = std::optional<sbx::physics::mesh_collider>{};

  if (node.has_component<sbx::physics::shape_collider>()) {
    // Narrowphase only resolves one collider per node (shape_collider wins; see
    // narrowphase.cpp's resolve_convex) — Add Component already blocks creating both.
    ImGui::TextColored(ImVec4{1.0f, 0.7f, 0.2f, 1.0f}, ICON_MDI_ALERT_OUTLINE " Also has a Shape Collider -- this one will be ignored.");
  }

  {
    const auto before = collider;

    ImGui::Text("Mesh:");
    ImGui::SameLine();

    if (draw_mesh_picker(state, "##mesh_collider_picker_popup", collider.mesh, assets_module)) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::physics::mesh_collider>>(node.id(), before, collider, "Edit Mesh Collider"));
    }
  }

  ImGui::Checkbox("Convex", &collider.is_convex);
  bracket_edit(state, target, node, collider, pending, "Edit Mesh Collider");

  if (!collider.is_convex) {
    ImGui::TextDisabled("Non-convex: only valid on a static or kinematic rigidbody.");
  }

  draw_collider_offset_rotation_friction(state, target, node, collider, pending, "Edit Mesh Collider");
}

auto draw_canvas_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_MONITOR " Canvas", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::canvas>>(node.id(), node.get_component<sbx::canvas::canvas>(), "Remove Canvas"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& canvas_component = node.get_component<sbx::canvas::canvas>();
  static auto pending = std::optional<sbx::canvas::canvas>{};

  static constexpr auto mode_labels = std::array<const char*, 3u>{"Screen Space - Overlay", "Screen Space - Camera", "World Space"};
  auto mode_index = static_cast<int>(canvas_component.mode);

  if (ImGui::Combo("Render Mode", &mode_index, mode_labels.data(), static_cast<int>(mode_labels.size()))) {
    canvas_component.mode = static_cast<sbx::canvas::render_mode>(mode_index);
  }
  bracket_edit(state, target, node, canvas_component, pending, "Edit Canvas");

  if (canvas_component.mode == sbx::canvas::render_mode::screen_space_camera) {
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
    auto& scene = scenes_module.active_scene();

    auto current_label = std::string{"(None)"};

    if (canvas_component.camera != sbx::math::uuid::nil()) {
      if (auto camera_node = scene.find(canvas_component.camera); camera_node.is_valid()) {
        current_label = camera_node.name().str();
      }
    }

    if (ImGui::BeginCombo("Camera", current_label.c_str())) {
      if (ImGui::Selectable("(None)", canvas_component.camera == sbx::math::uuid::nil())) {
        const auto before = canvas_component;
        canvas_component.camera = sbx::math::uuid::nil();
        state.push_command(target, std::make_unique<modify_component_command<sbx::canvas::canvas>>(node.id(), before, canvas_component, "Edit Canvas"));
      }

      for (auto&& [entity, camera_component] : scene.query<sbx::scenes::camera>().each()) {
        auto camera_node = scene.node_of(entity);
        const auto label = camera_node.name().str();
        const auto is_selected = camera_node.id() == canvas_component.camera;

        if (ImGui::Selectable(label.c_str(), is_selected)) {
          const auto before = canvas_component;
          canvas_component.camera = camera_node.id();
          state.push_command(target, std::make_unique<modify_component_command<sbx::canvas::canvas>>(node.id(), before, canvas_component, "Edit Canvas"));
        }
      }

      ImGui::EndCombo();
    }

    ImGui::DragFloat("Plane Distance", &canvas_component.plane_distance, 0.1f, 0.01f, 10000.0f);
    bracket_edit(state, target, node, canvas_component, pending, "Edit Canvas");
  } else if (canvas_component.mode == sbx::canvas::render_mode::world_space) {
    ImGui::TextDisabled("Projected through whichever camera is actually rendering");

    ImGui::DragFloat("World Scale", &canvas_component.world_scale, 0.0001f, 0.00001f, 10.0f, "%.5f");
    bracket_edit(state, target, node, canvas_component, pending, "Edit Canvas");

    ImGui::Checkbox("Billboard", &canvas_component.billboard);
    bracket_edit(state, target, node, canvas_component, pending, "Edit Canvas");
  }

  ImGui::DragInt("Sort Order", &canvas_component.sort_order);
  bracket_edit(state, target, node, canvas_component, pending, "Edit Canvas");
}

auto draw_canvas_group_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_OPACITY " Canvas Group", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::canvas_group>>(node.id(), node.get_component<sbx::canvas::canvas_group>(), "Remove Canvas Group"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& group = node.get_component<sbx::canvas::canvas_group>();
  static auto pending = std::optional<sbx::canvas::canvas_group>{};

  ImGui::DragFloat("Alpha", &group.alpha, 0.005f, 0.0f, 1.0f);
  bracket_edit(state, target, node, group, pending, "Edit Canvas Group");

  ImGui::Checkbox("Interactable", &group.interactable);
  bracket_edit(state, target, node, group, pending, "Edit Canvas Group");

  ImGui::Checkbox("Blocks Raycasts", &group.blocks_raycasts);
  bracket_edit(state, target, node, group, pending, "Edit Canvas Group");

  ImGui::Checkbox("Ignore Parent Groups", &group.ignore_parent_groups);
  bracket_edit(state, target, node, group, pending, "Edit Canvas Group");
}

auto draw_canvas_scaler_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_FIT_TO_SCREEN " Canvas Scaler", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::canvas_scaler>>(node.id(), node.get_component<sbx::canvas::canvas_scaler>(), "Remove Canvas Scaler"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& scaler = node.get_component<sbx::canvas::canvas_scaler>();
  static auto pending = std::optional<sbx::canvas::canvas_scaler>{};

  static constexpr auto mode_labels = std::array<const char*, 3u>{"Constant Pixel Size", "Scale With Screen Size", "Constant Physical Size"};
  auto mode_index = static_cast<int>(scaler.mode);

  if (ImGui::Combo("Scale Mode", &mode_index, mode_labels.data(), static_cast<int>(mode_labels.size()))) {
    scaler.mode = static_cast<sbx::canvas::canvas_scale_mode>(mode_index);
  }
  bracket_edit(state, target, node, scaler, pending, "Edit Canvas Scaler");

  if (scaler.mode == sbx::canvas::canvas_scale_mode::constant_physical_size) {
    ImGui::TextColored(ImVec4{1.0f, 0.7f, 0.2f, 1.0f}, ICON_MDI_ALERT_OUTLINE " Not implemented -- behaves like Constant Pixel Size.");
  }

  if (scaler.mode == sbx::canvas::canvas_scale_mode::scale_with_screen_size) {
    auto reference_resolution = std::array<std::float_t, 2u>{scaler.reference_resolution.x(), scaler.reference_resolution.y()};

    if (ImGui::DragFloat2("Reference Resolution", reference_resolution.data(), 1.0f, 1.0f, 16384.0f)) {
      scaler.reference_resolution = sbx::math::vector2{reference_resolution[0], reference_resolution[1]};
    }
    bracket_edit(state, target, node, scaler, pending, "Edit Canvas Scaler");

    ImGui::SliderFloat("Match Width Or Height", &scaler.match_width_or_height, 0.0f, 1.0f);
    bracket_edit(state, target, node, scaler, pending, "Edit Canvas Scaler");
  }
}

auto draw_rect_transform_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_ASPECT_RATIO " Rect Transform", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::rect_transform>>(node.id(), node.get_component<sbx::canvas::rect_transform>(), "Remove Rect Transform"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& rect = node.get_component<sbx::canvas::rect_transform>();
  static auto pending_before = std::optional<sbx::canvas::rect_transform>{};

  const auto capture_before = [&](const vector2_edit_result& result) {
    if (result.started && !pending_before) {
      pending_before = rect;
    }
  };

  const auto commit_after = [&](const vector2_edit_result& result) {
    if (result.committed && pending_before) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::canvas::rect_transform>>(node.id(), *pending_before, rect, "Edit Rect Transform"));
      pending_before.reset();
    }
  };

  auto anchor_min = std::array<std::float_t, 2u>{rect.anchor_min.x(), rect.anchor_min.y()};
  const auto anchor_min_result = draw_vector2_control("Anchor Min", anchor_min, 0.0f, 0.005f);
  capture_before(anchor_min_result);
  if (anchor_min_result.changed) {
    rect.anchor_min = sbx::math::vector2{anchor_min[0], anchor_min[1]};
  }
  commit_after(anchor_min_result);

  auto anchor_max = std::array<std::float_t, 2u>{rect.anchor_max.x(), rect.anchor_max.y()};
  const auto anchor_max_result = draw_vector2_control("Anchor Max", anchor_max, 1.0f, 0.005f);
  capture_before(anchor_max_result);
  if (anchor_max_result.changed) {
    rect.anchor_max = sbx::math::vector2{anchor_max[0], anchor_max[1]};
  }
  commit_after(anchor_max_result);

  auto anchored_position = std::array<std::float_t, 2u>{rect.anchored_position.x(), rect.anchored_position.y()};
  const auto anchored_position_result = draw_vector2_control("Anchored Position", anchored_position, 0.0f, 0.5f);
  capture_before(anchored_position_result);
  if (anchored_position_result.changed) {
    rect.anchored_position = sbx::math::vector2{anchored_position[0], anchored_position[1]};
  }
  commit_after(anchored_position_result);

  auto size_delta = std::array<std::float_t, 2u>{rect.size_delta.x(), rect.size_delta.y()};
  const auto size_delta_result = draw_vector2_control("Size Delta", size_delta, 100.0f, 0.5f);
  capture_before(size_delta_result);
  if (size_delta_result.changed) {
    rect.size_delta = sbx::math::vector2{size_delta[0], size_delta[1]};
  }
  commit_after(size_delta_result);

  auto pivot = std::array<std::float_t, 2u>{rect.pivot.x(), rect.pivot.y()};
  const auto pivot_result = draw_vector2_control("Pivot", pivot, 0.5f, 0.005f);
  capture_before(pivot_result);
  if (pivot_result.changed) {
    rect.pivot = sbx::math::vector2{pivot[0], pivot[1]};
  }
  commit_after(pivot_result);
}

auto draw_ui_image_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_IMAGE " UI Image", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::ui_image>>(node.id(), node.get_component<sbx::canvas::ui_image>(), "Remove UI Image"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& image = node.get_component<sbx::canvas::ui_image>();
  static auto pending = std::optional<sbx::canvas::ui_image>{};

  ImGui::Text("Sprite:");
  ImGui::SameLine();

  {
    const auto before = image;

    if (draw_texture_picker(state, "##ui_image_sprite_picker_popup", image.sprite, assets_module, sbx::graphics::format::r8g8b8a8_srgb)) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::canvas::ui_image>>(node.id(), before, image, "Edit UI Image"));
    }
  }

  draw_color_field("Tint", image.tint);
  bracket_edit(state, target, node, image, pending, "Edit UI Image");

  auto uv_rect = std::array<std::float_t, 4u>{image.uv_rect.x(), image.uv_rect.y(), image.uv_rect.z(), image.uv_rect.w()};

  if (ImGui::DragFloat4("UV Rect", uv_rect.data(), 0.01f)) {
    image.uv_rect = sbx::math::vector4{uv_rect[0], uv_rect[1], uv_rect[2], uv_rect[3]};
  }
  bracket_edit(state, target, node, image, pending, "Edit UI Image");

  ImGui::Checkbox("Raycast Target", &image.raycast_target);
  bracket_edit(state, target, node, image, pending, "Edit UI Image");
}

auto draw_ui_text_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_FORMAT_TEXT " UI Text", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::ui_text>>(node.id(), node.get_component<sbx::canvas::ui_text>(), "Remove UI Text"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& text = node.get_component<sbx::canvas::ui_text>();
  static auto pending = std::optional<sbx::canvas::ui_text>{};

  auto buffer = std::array<char, 512u>{};
  std::strncpy(buffer.data(), text.text.c_str(), buffer.size() - 1u);
  buffer[buffer.size() - 1u] = '\0';

  if (ImGui::InputTextMultiline("Text", buffer.data(), buffer.size(), ImVec2{0.0f, ImGui::GetTextLineHeight() * 3.0f})) {
    text.text = std::string{buffer.data()};
  }
  bracket_edit(state, target, node, text, pending, "Edit UI Text");

  ImGui::Text("Font:");
  ImGui::SameLine();

  {
    const auto before = text;

    if (draw_font_picker(state, "##ui_text_font_picker_popup", text.font)) {
      state.push_command(target, std::make_unique<modify_component_command<sbx::canvas::ui_text>>(node.id(), before, text, "Edit UI Text"));
    }
  }

  ImGui::DragFloat("Font Size", &text.font_size, 0.25f, 1.0f, 500.0f);
  bracket_edit(state, target, node, text, pending, "Edit UI Text");

  draw_color_field("Color", text.color);
  bracket_edit(state, target, node, text, pending, "Edit UI Text");

  static constexpr auto align_labels = std::array<const char*, 3u>{"Start", "Center", "End"};

  auto horizontal_index = static_cast<int>(text.horizontal_align);
  if (ImGui::Combo("Horizontal Align", &horizontal_index, align_labels.data(), static_cast<int>(align_labels.size()))) {
    text.horizontal_align = static_cast<sbx::canvas::text_align>(horizontal_index);
  }
  bracket_edit(state, target, node, text, pending, "Edit UI Text");

  auto vertical_index = static_cast<int>(text.vertical_align);
  if (ImGui::Combo("Vertical Align", &vertical_index, align_labels.data(), static_cast<int>(align_labels.size()))) {
    text.vertical_align = static_cast<sbx::canvas::text_align>(vertical_index);
  }
  bracket_edit(state, target, node, text, pending, "Edit UI Text");

  ImGui::DragFloat("Line Spacing", &text.line_spacing, 0.01f, 0.1f, 5.0f);
  bracket_edit(state, target, node, text, pending, "Edit UI Text");

  ImGui::Checkbox("Raycast Target", &text.raycast_target);
  bracket_edit(state, target, node, text, pending, "Edit UI Text");
}

auto draw_ui_button_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_GESTURE_TAP_BUTTON " UI Button", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::ui_button>>(node.id(), node.get_component<sbx::canvas::ui_button>(), "Remove UI Button"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& button = node.get_component<sbx::canvas::ui_button>();
  static auto pending = std::optional<sbx::canvas::ui_button>{};

  ImGui::Checkbox("Interactable", &button.interactable);
  bracket_edit(state, target, node, button, pending, "Edit UI Button");

  draw_color_field("Normal Color", button.normal_color);
  bracket_edit(state, target, node, button, pending, "Edit UI Button");

  draw_color_field("Hovered Color", button.hovered_color);
  bracket_edit(state, target, node, button, pending, "Edit UI Button");

  draw_color_field("Pressed Color", button.pressed_color);
  bracket_edit(state, target, node, button, pending, "Edit UI Button");
}

auto draw_layout_element_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_RULER " Layout Element", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::layout_element>>(node.id(), node.get_component<sbx::canvas::layout_element>(), "Remove Layout Element"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& element = node.get_component<sbx::canvas::layout_element>();
  static auto pending = std::optional<sbx::canvas::layout_element>{};

  ImGui::TextDisabled("-1 = unspecified (fall back to Rect Transform / group default)");

  ImGui::DragFloat("Min Width", &element.min_width, 0.5f, -1.0f, 10000.0f);
  bracket_edit(state, target, node, element, pending, "Edit Layout Element");
  ImGui::DragFloat("Min Height", &element.min_height, 0.5f, -1.0f, 10000.0f);
  bracket_edit(state, target, node, element, pending, "Edit Layout Element");
  ImGui::DragFloat("Preferred Width", &element.preferred_width, 0.5f, -1.0f, 10000.0f);
  bracket_edit(state, target, node, element, pending, "Edit Layout Element");
  ImGui::DragFloat("Preferred Height", &element.preferred_height, 0.5f, -1.0f, 10000.0f);
  bracket_edit(state, target, node, element, pending, "Edit Layout Element");
  ImGui::DragFloat("Flexible Width", &element.flexible_width, 0.05f, -1.0f, 100.0f);
  bracket_edit(state, target, node, element, pending, "Edit Layout Element");
  ImGui::DragFloat("Flexible Height", &element.flexible_height, 0.05f, -1.0f, 100.0f);
  bracket_edit(state, target, node, element, pending, "Edit Layout Element");

  ImGui::Checkbox("Ignore Layout", &element.ignore_layout);
  bracket_edit(state, target, node, element, pending, "Edit Layout Element");
}

auto draw_content_size_fitter_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_ARROW_COLLAPSE_ALL " Content Size Fitter", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::content_size_fitter>>(node.id(), node.get_component<sbx::canvas::content_size_fitter>(), "Remove Content Size Fitter"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& fitter = node.get_component<sbx::canvas::content_size_fitter>();
  static auto pending = std::optional<sbx::canvas::content_size_fitter>{};

  static constexpr auto fit_labels = std::array<const char*, 3u>{"Unconstrained", "Min Size", "Preferred Size"};

  auto horizontal_index = static_cast<int>(fitter.horizontal_fit);
  if (ImGui::Combo("Horizontal Fit", &horizontal_index, fit_labels.data(), static_cast<int>(fit_labels.size()))) {
    fitter.horizontal_fit = static_cast<sbx::canvas::content_fit_mode>(horizontal_index);
  }
  bracket_edit(state, target, node, fitter, pending, "Edit Content Size Fitter");

  auto vertical_index = static_cast<int>(fitter.vertical_fit);
  if (ImGui::Combo("Vertical Fit", &vertical_index, fit_labels.data(), static_cast<int>(fit_labels.size()))) {
    fitter.vertical_fit = static_cast<sbx::canvas::content_fit_mode>(vertical_index);
  }
  bracket_edit(state, target, node, fitter, pending, "Edit Content Size Fitter");
}

static constexpr auto layout_alignment_labels = std::array<const char*, 9u>{
  "Upper Left", "Upper Center", "Upper Right",
  "Middle Left", "Middle Center", "Middle Right",
  "Lower Left", "Lower Center", "Lower Right",
};

auto draw_horizontal_layout_group_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_VIEW_COLUMN " Horizontal Layout Group", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::horizontal_layout_group>>(node.id(), node.get_component<sbx::canvas::horizontal_layout_group>(), "Remove Horizontal Layout Group"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& group = node.get_component<sbx::canvas::horizontal_layout_group>();
  static auto pending = std::optional<sbx::canvas::horizontal_layout_group>{};

  ImGui::DragFloat("Spacing", &group.spacing, 0.5f, 0.0f, 1000.0f);
  bracket_edit(state, target, node, group, pending, "Edit Horizontal Layout Group");

  auto padding = std::array<std::float_t, 4u>{group.padding.x(), group.padding.y(), group.padding.z(), group.padding.w()};
  if (ImGui::DragFloat4("Padding (L,T,R,B)", padding.data(), 0.5f, 0.0f, 1000.0f)) {
    group.padding = sbx::math::vector4{padding[0], padding[1], padding[2], padding[3]};
  }
  bracket_edit(state, target, node, group, pending, "Edit Horizontal Layout Group");

  auto alignment_index = static_cast<int>(group.child_alignment);
  if (ImGui::Combo("Child Alignment", &alignment_index, layout_alignment_labels.data(), static_cast<int>(layout_alignment_labels.size()))) {
    group.child_alignment = static_cast<sbx::canvas::layout_alignment>(alignment_index);
  }
  bracket_edit(state, target, node, group, pending, "Edit Horizontal Layout Group");

  ImGui::Checkbox("Control Child Width", &group.control_child_width);
  bracket_edit(state, target, node, group, pending, "Edit Horizontal Layout Group");
  ImGui::Checkbox("Control Child Height", &group.control_child_height);
  bracket_edit(state, target, node, group, pending, "Edit Horizontal Layout Group");
  ImGui::Checkbox("Child Force Expand Width", &group.child_force_expand_width);
  bracket_edit(state, target, node, group, pending, "Edit Horizontal Layout Group");
  ImGui::Checkbox("Child Force Expand Height", &group.child_force_expand_height);
  bracket_edit(state, target, node, group, pending, "Edit Horizontal Layout Group");
}

auto draw_vertical_layout_group_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_VIEW_STREAM " Vertical Layout Group", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::vertical_layout_group>>(node.id(), node.get_component<sbx::canvas::vertical_layout_group>(), "Remove Vertical Layout Group"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& group = node.get_component<sbx::canvas::vertical_layout_group>();
  static auto pending = std::optional<sbx::canvas::vertical_layout_group>{};

  ImGui::DragFloat("Spacing", &group.spacing, 0.5f, 0.0f, 1000.0f);
  bracket_edit(state, target, node, group, pending, "Edit Vertical Layout Group");

  auto padding = std::array<std::float_t, 4u>{group.padding.x(), group.padding.y(), group.padding.z(), group.padding.w()};
  if (ImGui::DragFloat4("Padding (L,T,R,B)", padding.data(), 0.5f, 0.0f, 1000.0f)) {
    group.padding = sbx::math::vector4{padding[0], padding[1], padding[2], padding[3]};
  }
  bracket_edit(state, target, node, group, pending, "Edit Vertical Layout Group");

  auto alignment_index = static_cast<int>(group.child_alignment);
  if (ImGui::Combo("Child Alignment", &alignment_index, layout_alignment_labels.data(), static_cast<int>(layout_alignment_labels.size()))) {
    group.child_alignment = static_cast<sbx::canvas::layout_alignment>(alignment_index);
  }
  bracket_edit(state, target, node, group, pending, "Edit Vertical Layout Group");

  ImGui::Checkbox("Control Child Width", &group.control_child_width);
  bracket_edit(state, target, node, group, pending, "Edit Vertical Layout Group");
  ImGui::Checkbox("Control Child Height", &group.control_child_height);
  bracket_edit(state, target, node, group, pending, "Edit Vertical Layout Group");
  ImGui::Checkbox("Child Force Expand Width", &group.child_force_expand_width);
  bracket_edit(state, target, node, group, pending, "Edit Vertical Layout Group");
  ImGui::Checkbox("Child Force Expand Height", &group.child_force_expand_height);
  bracket_edit(state, target, node, group, pending, "Edit Vertical Layout Group");
}

auto draw_grid_layout_group_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_VIEW_GRID " Grid Layout Group", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::grid_layout_group>>(node.id(), node.get_component<sbx::canvas::grid_layout_group>(), "Remove Grid Layout Group"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& group = node.get_component<sbx::canvas::grid_layout_group>();
  static auto pending = std::optional<sbx::canvas::grid_layout_group>{};

  auto cell_size = std::array<std::float_t, 2u>{group.cell_size.x(), group.cell_size.y()};
  if (ImGui::DragFloat2("Cell Size", cell_size.data(), 0.5f, 1.0f, 10000.0f)) {
    group.cell_size = sbx::math::vector2{cell_size[0], cell_size[1]};
  }
  bracket_edit(state, target, node, group, pending, "Edit Grid Layout Group");

  auto spacing = std::array<std::float_t, 2u>{group.spacing.x(), group.spacing.y()};
  if (ImGui::DragFloat2("Spacing", spacing.data(), 0.5f, 0.0f, 1000.0f)) {
    group.spacing = sbx::math::vector2{spacing[0], spacing[1]};
  }
  bracket_edit(state, target, node, group, pending, "Edit Grid Layout Group");

  auto padding = std::array<std::float_t, 4u>{group.padding.x(), group.padding.y(), group.padding.z(), group.padding.w()};
  if (ImGui::DragFloat4("Padding (L,T,R,B)", padding.data(), 0.5f, 0.0f, 1000.0f)) {
    group.padding = sbx::math::vector4{padding[0], padding[1], padding[2], padding[3]};
  }
  bracket_edit(state, target, node, group, pending, "Edit Grid Layout Group");

  auto alignment_index = static_cast<int>(group.child_alignment);
  if (ImGui::Combo("Child Alignment", &alignment_index, layout_alignment_labels.data(), static_cast<int>(layout_alignment_labels.size()))) {
    group.child_alignment = static_cast<sbx::canvas::layout_alignment>(alignment_index);
  }
  bracket_edit(state, target, node, group, pending, "Edit Grid Layout Group");

  static constexpr auto corner_labels = std::array<const char*, 4u>{"Upper Left", "Upper Right", "Lower Left", "Lower Right"};
  auto corner_index = static_cast<int>(group.start_corner);
  if (ImGui::Combo("Start Corner", &corner_index, corner_labels.data(), static_cast<int>(corner_labels.size()))) {
    group.start_corner = static_cast<sbx::canvas::grid_start_corner>(corner_index);
  }
  bracket_edit(state, target, node, group, pending, "Edit Grid Layout Group");

  static constexpr auto axis_labels = std::array<const char*, 2u>{"Horizontal", "Vertical"};
  auto axis_index = static_cast<int>(group.start_axis);
  if (ImGui::Combo("Start Axis", &axis_index, axis_labels.data(), static_cast<int>(axis_labels.size()))) {
    group.start_axis = static_cast<sbx::canvas::grid_start_axis>(axis_index);
  }
  bracket_edit(state, target, node, group, pending, "Edit Grid Layout Group");

  static constexpr auto constraint_labels = std::array<const char*, 3u>{"Flexible", "Fixed Column Count", "Fixed Row Count"};
  auto constraint_index = static_cast<int>(group.constraint);
  if (ImGui::Combo("Constraint", &constraint_index, constraint_labels.data(), static_cast<int>(constraint_labels.size()))) {
    group.constraint = static_cast<sbx::canvas::grid_constraint>(constraint_index);
  }
  bracket_edit(state, target, node, group, pending, "Edit Grid Layout Group");

  if (group.constraint != sbx::canvas::grid_constraint::flexible) {
    ImGui::DragInt("Constraint Count", &group.constraint_count, 1.0f, 1, 1000);
    bracket_edit(state, target, node, group, pending, "Edit Grid Layout Group");
  }
}

auto draw_ui_mask_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_CROP " UI Mask", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::ui_mask>>(node.id(), node.get_component<sbx::canvas::ui_mask>(), "Remove UI Mask"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& mask = node.get_component<sbx::canvas::ui_mask>();
  static auto pending = std::optional<sbx::canvas::ui_mask>{};

  ImGui::Checkbox("Show Mask Graphic", &mask.show_mask_graphic);
  bracket_edit(state, target, node, mask, pending, "Edit UI Mask");
}

auto draw_ui_toggle_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_TOGGLE_SWITCH " UI Toggle", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::ui_toggle>>(node.id(), node.get_component<sbx::canvas::ui_toggle>(), "Remove UI Toggle"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& toggle = node.get_component<sbx::canvas::ui_toggle>();
  static auto pending = std::optional<sbx::canvas::ui_toggle>{};

  ImGui::Checkbox("Is On", &toggle.is_on);
  bracket_edit(state, target, node, toggle, pending, "Edit UI Toggle");

  ImGui::Checkbox("Interactable", &toggle.interactable);
  bracket_edit(state, target, node, toggle, pending, "Edit UI Toggle");

  draw_color_field("On Color", toggle.on_color);
  bracket_edit(state, target, node, toggle, pending, "Edit UI Toggle");

  draw_color_field("Off Color", toggle.off_color);
  bracket_edit(state, target, node, toggle, pending, "Edit UI Toggle");

  ImGui::Text("Group: %llu", static_cast<unsigned long long>(toggle.group.value()));
  ImGui::SameLine();

  if (ImGui::SmallButton("New##ToggleGroup")) {
    const auto before = toggle;
    toggle.group = sbx::math::uuid::create();
    state.push_command(target, std::make_unique<modify_component_command<sbx::canvas::ui_toggle>>(node.id(), before, toggle, "Edit UI Toggle"));
  }

  ImGui::SameLine();

  if (ImGui::SmallButton("Clear##ToggleGroup")) {
    const auto before = toggle;
    toggle.group = sbx::math::uuid::nil();
    state.push_command(target, std::make_unique<modify_component_command<sbx::canvas::ui_toggle>>(node.id(), before, toggle, "Edit UI Toggle"));
  }
}

static constexpr auto slider_direction_labels = std::array<const char*, 2u>{"Horizontal", "Vertical"};

auto draw_ui_slider_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_TUNE " UI Slider", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::ui_slider>>(node.id(), node.get_component<sbx::canvas::ui_slider>(), "Remove UI Slider"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& slider = node.get_component<sbx::canvas::ui_slider>();
  static auto pending = std::optional<sbx::canvas::ui_slider>{};

  ImGui::DragFloat("Value", &slider.value, 0.01f, slider.min_value, slider.max_value);
  bracket_edit(state, target, node, slider, pending, "Edit UI Slider");

  ImGui::DragFloat("Min Value", &slider.min_value, 0.1f);
  bracket_edit(state, target, node, slider, pending, "Edit UI Slider");

  ImGui::DragFloat("Max Value", &slider.max_value, 0.1f);
  bracket_edit(state, target, node, slider, pending, "Edit UI Slider");

  ImGui::Checkbox("Whole Numbers", &slider.whole_numbers);
  bracket_edit(state, target, node, slider, pending, "Edit UI Slider");

  ImGui::Checkbox("Interactable", &slider.interactable);
  bracket_edit(state, target, node, slider, pending, "Edit UI Slider");

  auto direction_index = static_cast<int>(slider.direction);
  if (ImGui::Combo("Direction", &direction_index, slider_direction_labels.data(), static_cast<int>(slider_direction_labels.size()))) {
    slider.direction = static_cast<sbx::canvas::slider_direction>(direction_index);
  }
  bracket_edit(state, target, node, slider, pending, "Edit UI Slider");

  draw_color_field("Track Color", slider.track_color);
  bracket_edit(state, target, node, slider, pending, "Edit UI Slider");

  draw_color_field("Fill Color", slider.fill_color);
  bracket_edit(state, target, node, slider, pending, "Edit UI Slider");
}

auto draw_ui_scrollbar_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_DRAG_HORIZONTAL " UI Scrollbar", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::ui_scrollbar>>(node.id(), node.get_component<sbx::canvas::ui_scrollbar>(), "Remove UI Scrollbar"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& scrollbar = node.get_component<sbx::canvas::ui_scrollbar>();
  static auto pending = std::optional<sbx::canvas::ui_scrollbar>{};

  ImGui::DragFloat("Value", &scrollbar.value, 0.01f, 0.0f, 1.0f);
  bracket_edit(state, target, node, scrollbar, pending, "Edit UI Scrollbar");

  ImGui::DragFloat("Size", &scrollbar.size, 0.01f, 0.0f, 1.0f);
  bracket_edit(state, target, node, scrollbar, pending, "Edit UI Scrollbar");

  ImGui::Checkbox("Interactable", &scrollbar.interactable);
  bracket_edit(state, target, node, scrollbar, pending, "Edit UI Scrollbar");

  auto direction_index = static_cast<int>(scrollbar.direction);
  if (ImGui::Combo("Direction", &direction_index, slider_direction_labels.data(), static_cast<int>(slider_direction_labels.size()))) {
    scrollbar.direction = static_cast<sbx::canvas::slider_direction>(direction_index);
  }
  bracket_edit(state, target, node, scrollbar, pending, "Edit UI Scrollbar");

  draw_color_field("Track Color", scrollbar.track_color);
  bracket_edit(state, target, node, scrollbar, pending, "Edit UI Scrollbar");

  draw_color_field("Handle Color", scrollbar.handle_color);
  bracket_edit(state, target, node, scrollbar, pending, "Edit UI Scrollbar");
}

auto draw_ui_scroll_rect_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void {
  auto is_open = true;

  const auto is_expanded = ImGui::CollapsingHeader(ICON_MDI_ARROW_ALL " UI Scroll Rect", &is_open, ImGuiTreeNodeFlags_DefaultOpen);

  if (!is_open) {
    state.push_command(target, std::make_unique<remove_component_command<sbx::canvas::ui_scroll_rect>>(node.id(), node.get_component<sbx::canvas::ui_scroll_rect>(), "Remove UI Scroll Rect"));
    return;
  }

  if (!is_expanded) {
    return;
  }

  auto& scroll = node.get_component<sbx::canvas::ui_scroll_rect>();
  static auto pending = std::optional<sbx::canvas::ui_scroll_rect>{};

  auto current_label = std::string{"(None)"};

  if (scroll.content != sbx::math::uuid::nil()) {
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
    auto& scene = scenes_module.active_scene();

    if (auto content_node = scene.find(scroll.content); content_node.is_valid()) {
      current_label = content_node.name().str();
    }
  }

  if (ImGui::BeginCombo("Content", current_label.c_str())) {
    if (ImGui::Selectable("(None)", scroll.content == sbx::math::uuid::nil())) {
      const auto before = scroll;
      scroll.content = sbx::math::uuid::nil();
      state.push_command(target, std::make_unique<modify_component_command<sbx::canvas::ui_scroll_rect>>(node.id(), before, scroll, "Edit UI Scroll Rect"));
    }

    if (node.has_component<sbx::scenes::relationship>()) {
      for (const auto child_entity : node.get_component<sbx::scenes::relationship>().children) {
        auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
        auto child_node = scenes_module.active_scene().node_of(child_entity);

        if (!child_node.has_component<sbx::canvas::rect_transform>()) {
          continue;
        }

        const auto label = child_node.name().str();
        const auto is_selected = child_node.id() == scroll.content;

        if (ImGui::Selectable(label.c_str(), is_selected)) {
          const auto before = scroll;
          scroll.content = child_node.id();
          state.push_command(target, std::make_unique<modify_component_command<sbx::canvas::ui_scroll_rect>>(node.id(), before, scroll, "Edit UI Scroll Rect"));
        }
      }
    }

    ImGui::EndCombo();
  }

  ImGui::Checkbox("Horizontal", &scroll.horizontal);
  bracket_edit(state, target, node, scroll, pending, "Edit UI Scroll Rect");

  ImGui::Checkbox("Vertical", &scroll.vertical);
  bracket_edit(state, target, node, scroll, pending, "Edit UI Scroll Rect");

  auto normalized_position = std::array<std::float_t, 2u>{scroll.normalized_position.x(), scroll.normalized_position.y()};
  if (ImGui::DragFloat2("Normalized Position", normalized_position.data(), 0.01f, 0.0f, 1.0f)) {
    scroll.normalized_position = sbx::math::vector2{normalized_position[0], normalized_position[1]};
  }
  bracket_edit(state, target, node, scroll, pending, "Edit UI Scroll Rect");
}

} // namespace editor
