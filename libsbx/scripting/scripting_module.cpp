// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/scripting_module.hpp>

#include <algorithm>
#include <array>

#include <fmt/core.h>
#include <fmt/args.h>
#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components.hpp>

#include <libsbx/canvas/components.hpp>

#include <libsbx/scripting/interop.hpp>
#include <libsbx/scripting/managed/field_info.hpp>

namespace sbx::scripting {

scripting_module::scripting_module() {
  const auto dotnet_dir = _dotnet_directory();

  auto config = scripting::managed::rumtime_config{
		.backend_path = dotnet_dir.generic_string(),
		.message_callback = nullptr,
		.exception_callback = _exception_callback
	};

  _runtime.initialize(config);

  _context = _runtime.create_assembly_load_context("ScriptingContext");

  auto core_assembly_path = std::filesystem::path{dotnet_dir / "Sbx.Core.dll"};

	_core_assembly = _context.load_assembly(core_assembly_path.string());

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Log_LogMessage", reinterpret_cast<void*>(&interop::log_log_message));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scripting_AttachScript", reinterpret_cast<void*>(&interop::scripting_attach_script));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scripting_GetInstance", reinterpret_cast<void*>(&interop::scripting_get_instance));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Behavior_AddComponent", reinterpret_cast<void*>(&interop::behavior_add_component));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Behavior_HasComponent", reinterpret_cast<void*>(&interop::behavior_has_component));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Behavior_RemoveComponent", reinterpret_cast<void*>(&interop::behavior_remove_component));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Tag_GetTag", reinterpret_cast<void*>(&interop::tag_get_tag));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Tag_SetTag", reinterpret_cast<void*>(&interop::tag_set_tag));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetPosition", reinterpret_cast<void*>(&interop::transform_get_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_SetPosition", reinterpret_cast<void*>(&interop::transform_set_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetWorldPosition", reinterpret_cast<void*>(&interop::transform_get_world_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetRotation", reinterpret_cast<void*>(&interop::transform_get_rotation));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_SetRotation", reinterpret_cast<void*>(&interop::transform_set_rotation));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetRight", reinterpret_cast<void*>(&interop::transform_get_right));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetForward", reinterpret_cast<void*>(&interop::transform_get_forward));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetUp", reinterpret_cast<void*>(&interop::transform_get_up));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_GetScale", reinterpret_cast<void*>(&interop::transform_get_scale));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_SetScale", reinterpret_cast<void*>(&interop::transform_set_scale));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Transform_LookAt", reinterpret_cast<void*>(&interop::transform_look_at));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Animator_GetPlaying", reinterpret_cast<void*>(&interop::animator_get_playing));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Animator_SetPlaying", reinterpret_cast<void*>(&interop::animator_set_playing));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Animator_GetCurrentStateName", reinterpret_cast<void*>(&interop::animator_get_current_state_name));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Animator_SetFloat", reinterpret_cast<void*>(&interop::animator_set_float));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Animator_SetBool", reinterpret_cast<void*>(&interop::animator_set_bool));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Animator_SetInt", reinterpret_cast<void*>(&interop::animator_set_int));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Animator_SetTrigger", reinterpret_cast<void*>(&interop::animator_set_trigger));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Animator_GetFloat", reinterpret_cast<void*>(&interop::animator_get_float));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Animator_GetBool", reinterpret_cast<void*>(&interop::animator_get_bool));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Animator_GetInt", reinterpret_cast<void*>(&interop::animator_get_int));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_GetLinearVelocity", reinterpret_cast<void*>(&interop::rigidbody_get_linear_velocity));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_SetLinearVelocity", reinterpret_cast<void*>(&interop::rigidbody_set_linear_velocity));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_GetAngularVelocity", reinterpret_cast<void*>(&interop::rigidbody_get_angular_velocity));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_SetAngularVelocity", reinterpret_cast<void*>(&interop::rigidbody_set_angular_velocity));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_GetMass", reinterpret_cast<void*>(&interop::rigidbody_get_mass));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_SetMass", reinterpret_cast<void*>(&interop::rigidbody_set_mass));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_GetGravityScale", reinterpret_cast<void*>(&interop::rigidbody_get_gravity_scale));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_SetGravityScale", reinterpret_cast<void*>(&interop::rigidbody_set_gravity_scale));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_AddForce", reinterpret_cast<void*>(&interop::rigidbody_add_force));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_AddTorque", reinterpret_cast<void*>(&interop::rigidbody_add_torque));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_FindByName", reinterpret_cast<void*>(&interop::node_find_by_name));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_Create", reinterpret_cast<void*>(&interop::node_create));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_InstantiatePrefab", reinterpret_cast<void*>(&interop::node_instantiate_prefab));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_Destroy", reinterpret_cast<void*>(&interop::node_destroy));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_SetParent", reinterpret_cast<void*>(&interop::node_set_parent));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_SetActive", reinterpret_cast<void*>(&interop::node_set_active));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_GetIsActive", reinterpret_cast<void*>(&interop::node_get_is_active));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_Load", reinterpret_cast<void*>(&interop::scene_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_Save", reinterpret_cast<void*>(&interop::scene_save));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_New", reinterpret_cast<void*>(&interop::scene_new));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_Load", reinterpret_cast<void*>(&interop::particle_effect_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_Play", reinterpret_cast<void*>(&interop::particle_effect_play));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_Pause", reinterpret_cast<void*>(&interop::particle_effect_pause));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_Stop", reinterpret_cast<void*>(&interop::particle_effect_stop));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_GetLoop", reinterpret_cast<void*>(&interop::particle_effect_get_loop));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_SetLoop", reinterpret_cast<void*>(&interop::particle_effect_set_loop));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_GetIsPlaying", reinterpret_cast<void*>(&interop::particle_effect_get_is_playing));

  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetHeight", reinterpret_cast<void*>(&interop::character_controller_get_height));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetRadius", reinterpret_cast<void*>(&interop::character_controller_get_radius));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetSlopeLimit", reinterpret_cast<void*>(&interop::character_controller_get_slope_limit));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetStepOffset", reinterpret_cast<void*>(&interop::character_controller_get_step_offset));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetIsGrounded", reinterpret_cast<void*>(&interop::character_controller_get_is_grounded));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetFlags", reinterpret_cast<void*>(&interop::character_controller_get_flags));
  // _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_Move", reinterpret_cast<void*>(&interop::character_controller_move));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsKeyPressed", reinterpret_cast<void*>(&interop::input_is_key_pressed));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsKeyDown", reinterpret_cast<void*>(&interop::input_is_key_down));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsKeyReleased", reinterpret_cast<void*>(&interop::input_is_key_released));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsMouseButtonPressed", reinterpret_cast<void*>(&interop::input_is_mouse_button_pressed));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsMouseButtonDown", reinterpret_cast<void*>(&interop::input_is_mouse_button_down));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_IsMouseButtonReleased", reinterpret_cast<void*>(&interop::input_is_mouse_button_released));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_MousePosition", reinterpret_cast<void*>(&interop::input_mouse_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Input_ScrollDelta", reinterpret_cast<void*>(&interop::input_scroll_delta));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_ScreenPointToRay", reinterpret_cast<void*>(&interop::camera_screen_point_to_ray));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_WorldToScreenPoint", reinterpret_cast<void*>(&interop::camera_world_to_screen_point));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_MainGetPosition", reinterpret_cast<void*>(&interop::camera_main_get_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_MainSetPosition", reinterpret_cast<void*>(&interop::camera_main_set_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_MainGetRotation", reinterpret_cast<void*>(&interop::camera_main_get_rotation));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_MainSetRotation", reinterpret_cast<void*>(&interop::camera_main_set_rotation));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_MainGetForward", reinterpret_cast<void*>(&interop::camera_main_get_forward));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_MainGetRight", reinterpret_cast<void*>(&interop::camera_main_get_right));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_MainGetUp", reinterpret_cast<void*>(&interop::camera_main_get_up));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetViewport", reinterpret_cast<void*>(&interop::camera_get_viewport));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetViewportOffset", reinterpret_cast<void*>(&interop::camera_get_viewport_offset));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RenderSettings_SetWireframeEnabled", reinterpret_cast<void*>(&interop::render_settings_set_wireframe_enabled));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RenderSettings_GetWireframeEnabled", reinterpret_cast<void*>(&interop::render_settings_get_wireframe_enabled));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetFovDegrees", reinterpret_cast<void*>(&interop::camera_get_fov_degrees));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetFovDegrees", reinterpret_cast<void*>(&interop::camera_set_fov_degrees));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetNearPlane", reinterpret_cast<void*>(&interop::camera_get_near_plane));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetNearPlane", reinterpret_cast<void*>(&interop::camera_set_near_plane));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetFarPlane", reinterpret_cast<void*>(&interop::camera_get_far_plane));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetFarPlane", reinterpret_cast<void*>(&interop::camera_set_far_plane));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetExposure", reinterpret_cast<void*>(&interop::camera_get_exposure));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetExposure", reinterpret_cast<void*>(&interop::camera_set_exposure));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Time_DeltaTime", reinterpret_cast<void*>(&interop::time_delta_time));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Physics_Raycast", reinterpret_cast<void*>(&interop::physics_raycast));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Nav_Bake", reinterpret_cast<void*>(&interop::nav_bake));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Nav_HasNavMesh", reinterpret_cast<void*>(&interop::nav_has_navmesh));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Nav_SamplePosition", reinterpret_cast<void*>(&interop::nav_sample_position));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_SetDestination", reinterpret_cast<void*>(&interop::nav_agent_set_destination));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_GetState", reinterpret_cast<void*>(&interop::nav_agent_get_state));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_GetVelocity", reinterpret_cast<void*>(&interop::nav_agent_get_velocity));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_GetRemainingDistance", reinterpret_cast<void*>(&interop::nav_agent_get_remaining_distance));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_GetRadius", reinterpret_cast<void*>(&interop::nav_agent_get_radius));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_SetRadius", reinterpret_cast<void*>(&interop::nav_agent_set_radius));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_GetHeight", reinterpret_cast<void*>(&interop::nav_agent_get_height));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_SetHeight", reinterpret_cast<void*>(&interop::nav_agent_set_height));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_GetBaseOffset", reinterpret_cast<void*>(&interop::nav_agent_get_base_offset));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_SetBaseOffset", reinterpret_cast<void*>(&interop::nav_agent_set_base_offset));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_GetSpeed", reinterpret_cast<void*>(&interop::nav_agent_get_speed));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_SetSpeed", reinterpret_cast<void*>(&interop::nav_agent_set_speed));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_GetAcceleration", reinterpret_cast<void*>(&interop::nav_agent_get_acceleration));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "NavAgent_SetAcceleration", reinterpret_cast<void*>(&interop::nav_agent_set_acceleration));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Terrain_Generate", reinterpret_cast<void*>(&interop::terrain_generate));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Terrain_SampleHeight", reinterpret_cast<void*>(&interop::terrain_sample_height));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Terrain_SampleNormal", reinterpret_cast<void*>(&interop::terrain_sample_normal));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Math_NoiseSimplex", reinterpret_cast<void*>(&interop::math_noise_simplex));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Math_NoiseFractal", reinterpret_cast<void*>(&interop::math_noise_fractal));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "MeshRenderer_SetGeometry", reinterpret_cast<void*>(&interop::mesh_renderer_set_geometry));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "MeshRenderer_SetMaterial", reinterpret_cast<void*>(&interop::mesh_renderer_set_material));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_Load", reinterpret_cast<void*>(&interop::material_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_CreateInstance", reinterpret_cast<void*>(&interop::material_create_instance));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_SetTexture", reinterpret_cast<void*>(&interop::material_set_texture));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_Release", reinterpret_cast<void*>(&interop::material_release));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_IsLoaded", reinterpret_cast<void*>(&interop::material_is_loaded));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_SampleBilinear", reinterpret_cast<void*>(&interop::texture_sample_bilinear));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_Load", reinterpret_cast<void*>(&interop::texture_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_CreateStorageImage", reinterpret_cast<void*>(&interop::texture_create_storage_image));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_ReadPixels", reinterpret_cast<void*>(&interop::texture_read_pixels));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_Release", reinterpret_cast<void*>(&interop::texture_release));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Debug_WritePng", reinterpret_cast<void*>(&interop::debug_write_png));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_IsResident", reinterpret_cast<void*>(&interop::texture_is_resident));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeBuffer_Create", reinterpret_cast<void*>(&interop::compute_buffer_create));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeBuffer_SetData", reinterpret_cast<void*>(&interop::compute_buffer_set_data));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeBuffer_GetData", reinterpret_cast<void*>(&interop::compute_buffer_get_data));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeBuffer_Release", reinterpret_cast<void*>(&interop::compute_buffer_release));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeShader_Load", reinterpret_cast<void*>(&interop::compute_shader_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeShader_SetValue", reinterpret_cast<void*>(&interop::compute_shader_set_value));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeShader_SetTexture", reinterpret_cast<void*>(&interop::compute_shader_set_texture));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeShader_SetBuffer", reinterpret_cast<void*>(&interop::compute_shader_set_buffer));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeShader_Release", reinterpret_cast<void*>(&interop::compute_shader_release));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeCommands_Begin", reinterpret_cast<void*>(&interop::compute_commands_begin));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeCommands_Dispatch", reinterpret_cast<void*>(&interop::compute_commands_dispatch));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeCommands_Submit", reinterpret_cast<void*>(&interop::compute_commands_submit));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeCommands_IsComplete", reinterpret_cast<void*>(&interop::compute_commands_is_complete));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ComputeCommands_Release", reinterpret_cast<void*>(&interop::compute_commands_release));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Canvas_GetSortOrder", reinterpret_cast<void*>(&interop::canvas_get_sort_order));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Canvas_SetSortOrder", reinterpret_cast<void*>(&interop::canvas_set_sort_order));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Canvas_WantsPointerCapture", reinterpret_cast<void*>(&interop::canvas_wants_pointer_capture));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RectTransform_GetAnchorMin", reinterpret_cast<void*>(&interop::rect_transform_get_anchor_min));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RectTransform_SetAnchorMin", reinterpret_cast<void*>(&interop::rect_transform_set_anchor_min));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RectTransform_GetAnchorMax", reinterpret_cast<void*>(&interop::rect_transform_get_anchor_max));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RectTransform_SetAnchorMax", reinterpret_cast<void*>(&interop::rect_transform_set_anchor_max));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RectTransform_GetAnchoredPosition", reinterpret_cast<void*>(&interop::rect_transform_get_anchored_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RectTransform_SetAnchoredPosition", reinterpret_cast<void*>(&interop::rect_transform_set_anchored_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RectTransform_GetSizeDelta", reinterpret_cast<void*>(&interop::rect_transform_get_size_delta));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RectTransform_SetSizeDelta", reinterpret_cast<void*>(&interop::rect_transform_set_size_delta));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RectTransform_GetPivot", reinterpret_cast<void*>(&interop::rect_transform_get_pivot));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RectTransform_SetPivot", reinterpret_cast<void*>(&interop::rect_transform_set_pivot));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIImage_GetTint", reinterpret_cast<void*>(&interop::ui_image_get_tint));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIImage_SetTint", reinterpret_cast<void*>(&interop::ui_image_set_tint));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIImage_LoadSprite", reinterpret_cast<void*>(&interop::ui_image_load_sprite));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIText_GetText", reinterpret_cast<void*>(&interop::ui_text_get_text));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIText_SetText", reinterpret_cast<void*>(&interop::ui_text_set_text));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIText_GetFontSize", reinterpret_cast<void*>(&interop::ui_text_get_font_size));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIText_SetFontSize", reinterpret_cast<void*>(&interop::ui_text_set_font_size));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIText_GetColor", reinterpret_cast<void*>(&interop::ui_text_get_color));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIText_SetColor", reinterpret_cast<void*>(&interop::ui_text_set_color));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIText_LoadFont", reinterpret_cast<void*>(&interop::ui_text_load_font));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_GetInteractable", reinterpret_cast<void*>(&interop::ui_button_get_interactable));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_SetInteractable", reinterpret_cast<void*>(&interop::ui_button_set_interactable));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_GetNormalColor", reinterpret_cast<void*>(&interop::ui_button_get_normal_color));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_SetNormalColor", reinterpret_cast<void*>(&interop::ui_button_set_normal_color));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_GetHoveredColor", reinterpret_cast<void*>(&interop::ui_button_get_hovered_color));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_SetHoveredColor", reinterpret_cast<void*>(&interop::ui_button_set_hovered_color));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_GetPressedColor", reinterpret_cast<void*>(&interop::ui_button_get_pressed_color));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_SetPressedColor", reinterpret_cast<void*>(&interop::ui_button_set_pressed_color));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_GetIsHovered", reinterpret_cast<void*>(&interop::ui_button_get_is_hovered));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_GetIsPressed", reinterpret_cast<void*>(&interop::ui_button_get_is_pressed));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIButton_GetWasClicked", reinterpret_cast<void*>(&interop::ui_button_get_was_clicked));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CanvasGroup_GetAlpha", reinterpret_cast<void*>(&interop::canvas_group_get_alpha));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CanvasGroup_SetAlpha", reinterpret_cast<void*>(&interop::canvas_group_set_alpha));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CanvasGroup_GetInteractable", reinterpret_cast<void*>(&interop::canvas_group_get_interactable));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CanvasGroup_SetInteractable", reinterpret_cast<void*>(&interop::canvas_group_set_interactable));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CanvasGroup_GetBlocksRaycasts", reinterpret_cast<void*>(&interop::canvas_group_get_blocks_raycasts));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CanvasGroup_SetBlocksRaycasts", reinterpret_cast<void*>(&interop::canvas_group_set_blocks_raycasts));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CanvasGroup_GetIgnoreParentGroups", reinterpret_cast<void*>(&interop::canvas_group_get_ignore_parent_groups));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CanvasGroup_SetIgnoreParentGroups", reinterpret_cast<void*>(&interop::canvas_group_set_ignore_parent_groups));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIToggle_GetIsOn", reinterpret_cast<void*>(&interop::ui_toggle_get_is_on));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIToggle_SetIsOn", reinterpret_cast<void*>(&interop::ui_toggle_set_is_on));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIToggle_GetInteractable", reinterpret_cast<void*>(&interop::ui_toggle_get_interactable));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIToggle_SetInteractable", reinterpret_cast<void*>(&interop::ui_toggle_set_interactable));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UISlider_GetValue", reinterpret_cast<void*>(&interop::ui_slider_get_value));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UISlider_SetValue", reinterpret_cast<void*>(&interop::ui_slider_set_value));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UISlider_GetMinValue", reinterpret_cast<void*>(&interop::ui_slider_get_min_value));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UISlider_SetMinValue", reinterpret_cast<void*>(&interop::ui_slider_set_min_value));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UISlider_GetMaxValue", reinterpret_cast<void*>(&interop::ui_slider_get_max_value));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UISlider_SetMaxValue", reinterpret_cast<void*>(&interop::ui_slider_set_max_value));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UISlider_GetWholeNumbers", reinterpret_cast<void*>(&interop::ui_slider_get_whole_numbers));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UISlider_SetWholeNumbers", reinterpret_cast<void*>(&interop::ui_slider_set_whole_numbers));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UISlider_GetInteractable", reinterpret_cast<void*>(&interop::ui_slider_get_interactable));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UISlider_SetInteractable", reinterpret_cast<void*>(&interop::ui_slider_set_interactable));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollbar_GetValue", reinterpret_cast<void*>(&interop::ui_scrollbar_get_value));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollbar_SetValue", reinterpret_cast<void*>(&interop::ui_scrollbar_set_value));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollbar_GetSize", reinterpret_cast<void*>(&interop::ui_scrollbar_get_size));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollbar_SetSize", reinterpret_cast<void*>(&interop::ui_scrollbar_set_size));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollbar_GetInteractable", reinterpret_cast<void*>(&interop::ui_scrollbar_get_interactable));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollbar_SetInteractable", reinterpret_cast<void*>(&interop::ui_scrollbar_set_interactable));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollRect_GetNormalizedPosition", reinterpret_cast<void*>(&interop::ui_scroll_rect_get_normalized_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollRect_SetNormalizedPosition", reinterpret_cast<void*>(&interop::ui_scroll_rect_set_normalized_position));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollRect_GetHorizontal", reinterpret_cast<void*>(&interop::ui_scroll_rect_get_horizontal));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollRect_SetHorizontal", reinterpret_cast<void*>(&interop::ui_scroll_rect_set_horizontal));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollRect_GetVertical", reinterpret_cast<void*>(&interop::ui_scroll_rect_get_vertical));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollRect_SetVertical", reinterpret_cast<void*>(&interop::ui_scroll_rect_set_vertical));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIMask_GetShowMaskGraphic", reinterpret_cast<void*>(&interop::ui_mask_get_show_mask_graphic));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIMask_SetShowMaskGraphic", reinterpret_cast<void*>(&interop::ui_mask_set_show_mask_graphic));

  interop::register_managed_component<scenes::tag>("Sbx.Core.Components.Tag", _core_assembly);
  interop::register_managed_component<scenes::local_transform>("Sbx.Core.Components.Transform", _core_assembly);
  interop::register_managed_component<scenes::animator>("Sbx.Core.Components.Animator", _core_assembly);
  interop::register_managed_component<scenes::particle_effect>("Sbx.Core.Components.ParticleEffect", _core_assembly);
  interop::register_managed_component<scenes::camera>("Sbx.Core.Components.CameraSettings", _core_assembly);
  interop::register_managed_component<physics::rigidbody>("Sbx.Core.Physics.Rigidbody", _core_assembly);
  interop::register_managed_component<physics::nav_agent>("Sbx.Core.Physics.NavAgent", _core_assembly);
  interop::register_managed_component<scenes::mesh_renderer>("Sbx.Core.Components.MeshRenderer", _core_assembly);
  interop::register_managed_component<canvas::canvas>("Sbx.Core.UI.Canvas", _core_assembly);
  interop::register_managed_component<canvas::rect_transform>("Sbx.Core.UI.RectTransform", _core_assembly);
  interop::register_managed_component<canvas::ui_image>("Sbx.Core.UI.UIImage", _core_assembly);
  interop::register_managed_component<canvas::ui_text>("Sbx.Core.UI.UIText", _core_assembly);
  interop::register_managed_component<canvas::ui_button>("Sbx.Core.UI.UIButton", _core_assembly);
  interop::register_managed_component<canvas::canvas_group>("Sbx.Core.UI.CanvasGroup", _core_assembly);
  interop::register_managed_component<canvas::ui_toggle>("Sbx.Core.UI.UIToggle", _core_assembly);
  interop::register_managed_component<canvas::ui_slider>("Sbx.Core.UI.UISlider", _core_assembly);
  interop::register_managed_component<canvas::ui_scrollbar>("Sbx.Core.UI.UIScrollbar", _core_assembly);
  interop::register_managed_component<canvas::ui_scroll_rect>("Sbx.Core.UI.UIScrollRect", _core_assembly);
  interop::register_managed_component<canvas::ui_mask>("Sbx.Core.UI.UIMask", _core_assembly);
  // interop::register_managed_component<physics::character_controller>("Sbx.Core.Physics.CharacterController", _core_assembly);

  _core_assembly.upload_internal_calls();

  auto& physics_module = core::engine::get_module<physics::physics_module>();

  physics_module.on_contact_began().connect([this](const physics::collision_event& event) { _dispatch_collision_event(event, true); });
  physics_module.on_contact_ended().connect([this](const physics::collision_event& event) { _dispatch_collision_event(event, false); });

  auto& canvas_module = core::engine::get_module<canvas::canvas_module>();

  canvas_module.on_button_clicked().connect([this](const scenes::node& node) { _dispatch_button_click(node); });
  canvas_module.on_value_changed().connect([this](const scenes::node& node) { _dispatch_value_changed(node); });

  _load_game_assembly();
}

scripting_module::~scripting_module() {
  _runtime.shutdown();
}

auto scripting_module::update() -> void {
  SBX_PROFILE_SCOPE("scripting_module::update");

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  if (!scenes_module.is_simulating()) {
    return;
  }

  auto& scene = scenes_module.active_scene();

  // Mirrors Unity: a disabled node's scripts stop receiving OnUpdate (OnCreate/OnDestroy still
  // fire regardless -- see instantiate_scene_scripts/node_destroy, which never check active state).
  auto scripts_query = scene.query<scripting::scripts>(ecs::exclude<scenes::inactive>);

  for (auto&& [node, scripts] : scripts_query.each()) {
    for (auto& instance : scripts.instances) {
      instance.invoke("OnUpdate");
    }
  }
}

auto scripting_module::load_assembly(const std::filesystem::path& assembly_path, std::initializer_list<internal_call> bindings) -> void {
  _assembly_path = assembly_path;

  auto& assembly = _context.get_or_load_assembly(_assembly_path.string());

  for (const auto& binding : bindings) {
    assembly.add_internal_call(binding.type_name, binding.method_name, binding.function);
  }

  assembly.upload_internal_calls();

  utility::logger<"scripting">::info("Loaded game assembly '{}' with {} bindings", assembly_path.string(), bindings.size());
}

auto scripting_module::instantiate(scenes::node& node, std::string_view class_name) -> managed::object {
  if (!_has_game_assembly) {
    utility::logger<"scripting">::error("Cannot instantiate '{}' — no compiled game assembly is loaded", class_name);
    return managed::object{};
  }

  auto& type = _game_assembly.get_type(class_name);
  auto instance = type.create_instance(node.get_component<scenes::id>().value());

  if (!instance.is_valid()) {
    utility::logger<"scripting">::error("Failed to instantiate script '{}' — construction failed on the C# side (see the exception logged above)", class_name);
    return managed::object{};
  }

  if (node.has_component<scenes::script_component>()) {
    const auto& persisted = node.get_component<scenes::script_component>();
    for (const auto& entry : persisted.scripts) {
      if (entry.class_name == class_name) {
        _apply_field_overrides(instance, entry);
        break;
      }
    }
  }

  auto& scripts = node.get_or_add_component<scripting::scripts>();
  scripts.instances.push_back(instance);

  return instance;
}

auto scripting_module::instantiate_scene_scripts(scenes::scene& target) -> void {
  // Phase 1: Create and register all script instances across all nodes first
  for (auto&& [entity, list] : target.query<scenes::script_component>().each()) {
    auto node = target.node_of(entity);

    for (const auto& entry : list.scripts) {
      instantiate(node, entry.class_name);
    }
  }

  // Phase 2: Invoke OnCreate for all initialized instances safely
  for (auto&& [entity, scripts] : target.query<scripting::scripts>().each()) {
    for (auto& instance : scripts.instances) {
      instance.invoke("OnCreate");
    }
  }
}

auto scripting_module::instantiate_subtree_scripts(scenes::scene& target, scenes::node subtree_root) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  if (!scenes_module.is_simulating()) {
    return;
  }

  // Phase 1: create and register every script instance across the whole subtree first (same
  // ordering rationale as instantiate_scene_scripts — a sibling's OnCreate might look this node's
  // component up via GetComponent before every instance in the subtree exists otherwise).
  auto created = std::vector<managed::object>{};

  const auto collect = [&](this const auto& self, scenes::node current) -> void {
    if (const auto* list = current.try_get_component<scenes::script_component>().get()) {
      for (const auto& entry : list->scripts) {
        created.push_back(instantiate(current, entry.class_name));
      }
    }

    for (const auto child : current.get_component<scenes::relationship>().children) {
      self(target.node_of(child));
    }
  };

  collect(subtree_root);

  // Phase 2: invoke OnCreate for all initialized instances safely.
  for (auto& instance : created) {
    instance.invoke("OnCreate");
  }
}

auto scripting_module::attach_script(scenes::node& node, std::string_view class_name) -> void {
  auto& scripts = node.get_or_add_component<scenes::script_component>();

  const auto already_attached = std::ranges::any_of(scripts.scripts, [&](const auto& entry) {
    return entry.class_name == class_name;
  });

  if (already_attached) {
    return;
  }

  scripts.scripts.push_back(scenes::script_entry{.class_name = std::string{class_name}});

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  if (scenes_module.is_simulating()) {
    auto instance = instantiate(node, class_name);
    instance.invoke("OnCreate");
  } else {
    seed_missing_field_defaults(node, scripts.scripts.back());
  }
}

auto scripting_module::seed_missing_field_defaults(scenes::node& node, scenes::script_entry& entry) -> void {
  if (!_has_game_assembly) {
    return;
  }

  auto& type = _game_assembly.get_type(entry.class_name);

  if (!type) {
    return;
  }

  const auto already_seeded = [&](std::string_view field_name) {
    return std::ranges::any_of(entry.field_overrides, [&](const auto& existing) { return existing.name == field_name; });
  };

  // Cheap up-front check so a script with nothing missing (the overwhelmingly common case once
  // this has run once) never pays for constructing a scratch instance at all -- this runs on
  // every Inspector draw of a script section (see draw_script_field_inspector), not just attach.
  const auto has_missing = std::ranges::any_of(type.get_fields(), [&](auto& field) {
    return field.get_accessibility() == managed::type_accessibility::public_access
        && script_field_type_of(std::string{field.get_type().get_full_name()})
        && !already_seeded(std::string{field.get_name()});
  });

  if (!has_missing) {
    return;
  }

  // Construct one just long enough to read its real field defaults, straight into
  // field_overrides, then throw it away -- never registered into `scripts`, no OnCreate. Assumes a
  // script's real setup happens in OnCreate, not its constructor (same convention Unity's
  // MonoBehaviour uses); a constructor with actual side effects would run them here too.
  auto scratch = type.create_instance(node.get_component<scenes::id>().value());

  if (!scratch.is_valid()) {
    utility::logger<"scripting">::warn("Could not construct '{}' to read its script field defaults", entry.class_name);
    return;
  }

  for (auto& field : type.get_fields()) {
    if (field.get_accessibility() != managed::type_accessibility::public_access) {
      continue;
    }

    const auto field_type = script_field_type_of(std::string{field.get_type().get_full_name()});

    if (!field_type) {
      continue;
    }

    const auto field_name = std::string{field.get_name()};

    if (already_seeded(field_name)) {
      continue;
    }

    auto default_value = scenes::script_field_override{.name = field_name, .type = *field_type};

    switch (*field_type) {
      case scenes::script_field_type::float32: default_value.float_value = scratch.get_field_value<std::float_t>(field_name); break;
      case scenes::script_field_type::int32:   default_value.int_value = scratch.get_field_value<std::int32_t>(field_name); break;
      case scenes::script_field_type::boolean: default_value.bool_value = scratch.get_field_value<bool>(field_name); break;
      case scenes::script_field_type::string:  default_value.string_value = scratch.get_field_value<std::string>(field_name); break;
      case scenes::script_field_type::vector3: default_value.vector3_value = scratch.get_field_value<math::vector3>(field_name); break;
      case scenes::script_field_type::node:    default_value.node_value = math::uuid::from_value(scratch.get_field_value<std::uint64_t>(field_name)); break;
      case scenes::script_field_type::layer_mask: default_value.layer_mask_value = scratch.get_field_value<std::uint32_t>(field_name); break;
      case scenes::script_field_type::material: default_value.material_value = math::uuid::from_value(scratch.get_field_value<std::uint64_t>(field_name)); break;
      case scenes::script_field_type::color: default_value.color_value = scratch.get_field_value<math::color>(field_name); break;
    }

    entry.field_overrides.push_back(std::move(default_value));
  }

  scratch.destroy();
}

auto scripting_module::detach_script(scenes::node& node, std::string_view class_name) -> void {
  if (!node.has_component<scenes::script_component>()) {
    return;
  }

  auto& persisted = node.get_component<scenes::script_component>();

  std::erase_if(persisted.scripts, [&](const auto& entry) {
    return entry.class_name == class_name;
  });

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  if (scenes_module.is_simulating() && node.has_component<scripting::scripts>()) {
    auto& runtime_scripts = node.get_component<scripting::scripts>();

    std::erase_if(runtime_scripts.instances, [&](auto& instance) {
      if (instance.get_type().get_full_name() == class_name) {
        instance.invoke("OnDestroy");
        instance.destroy();
        return true;
      }

      return false;
    });
  }
}

auto scripting_module::_apply_field_overrides(managed::object& instance, const scenes::script_entry& entry) -> void {
  for (const auto& field : entry.field_overrides) {
    switch (field.type) {
      case scenes::script_field_type::float32: instance.set_field_value(field.name, field.float_value); break;
      case scenes::script_field_type::int32:   instance.set_field_value(field.name, field.int_value); break;
      case scenes::script_field_type::boolean: instance.set_field_value(field.name, field.bool_value); break;
      case scenes::script_field_type::string:  instance.set_field_value(field.name, field.string_value); break;
      case scenes::script_field_type::vector3: instance.set_field_value(field.name, field.vector3_value); break;
      // A Sbx.Core.Node-typed field isn't a blittable value the generic marshaling path can copy
      // (it's a managed reference) -- both directions cross as a raw uuid instead, via Node's own
      // INativeHandle implementation (see Sbx.Managed's SetFieldValue).
      case scenes::script_field_type::node:    instance.set_field_value(field.name, field.node_value.value()); break;
      // A Sbx.Core.Physics.LayerMask field is a blittable struct (one uint) -- same direct path as vector3.
      case scenes::script_field_type::layer_mask: instance.set_field_value(field.name, field.layer_mask_value); break;
      // A Sbx.Core.Material field is a managed reference, same INativeHandle uuid convention as node above.
      case scenes::script_field_type::material: instance.set_field_value(field.name, field.material_value.value()); break;
      // A Sbx.Core.Math.Color field is a blittable struct (four sequential floats), same direct path as vector3.
      case scenes::script_field_type::color: instance.set_field_value(field.name, field.color_value); break;
    }
  }
}

auto scripting_module::_invoke_collision_handler(scenes::node& self, const scenes::node& other, const physics::collision_event& event, bool began) -> void {
  if (!self.is_valid()) {
    return;
  }

  auto scripts = self.try_get_component<scripting::scripts>();

  if (!scripts) {
    return;
  }

  const auto other_uuid = other.is_valid() ? other.get_component<scenes::id>().value() : std::uint64_t{0u};

  // Indexed by (is_trigger << 1) | began -- avoids branching on two independent bools to pick one of four fixed strings.
  static constexpr auto dispatch_method_names = std::array<const char*, 4u>{
    "DispatchCollisionExit",
    "DispatchCollisionEnter",
    "DispatchTriggerExit",
    "DispatchTriggerEnter",
  };

  const auto method = dispatch_method_names[(static_cast<std::uint32_t>(event.is_trigger) << 1) | static_cast<std::uint32_t>(began)];

  for (auto& instance : scripts->instances) {
    instance.invoke(method, other_uuid, event.normal.x(), event.normal.y(), event.normal.z(), event.point.x(), event.point.y(), event.point.z());
  }
}

auto scripting_module::_dispatch_collision_event(const physics::collision_event& event, bool began) -> void {
  auto node_a = event.node_a;
  auto node_b = event.node_b;

  _invoke_collision_handler(node_a, node_b, event, began);
  _invoke_collision_handler(node_b, node_a, event, began);
}

auto scripting_module::_dispatch_button_click(const scenes::node& node) -> void {
  if (!node.is_valid()) {
    return;
  }

  auto scripts = node.try_get_component<scripting::scripts>();

  if (!scripts) {
    return;
  }

  for (const auto& instance : scripts->instances) {
    instance.invoke("OnClick");
  }
}

auto scripting_module::_dispatch_value_changed(const scenes::node& node) -> void {
  if (!node.is_valid()) {
    return;
  }

  auto scripts = node.try_get_component<scripting::scripts>();

  if (!scripts) {
    return;
  }

  for (const auto& instance : scripts->instances) {
    instance.invoke("OnValueChanged");
  }
}

auto scripting_module::run_on_destroy(scenes::scene& target) -> void {
  auto scripts_query = target.query<scripting::scripts>();

  for (auto&& [node, scripts] : scripts_query.each()) {
    for (auto& instance : scripts.instances) {
      instance.invoke("OnDestroy");
      instance.destroy();
    }
  }
}

auto scripting_module::recompile_scripts() -> void {
  _load_game_assembly();
}

auto scripting_module::_dotnet_directory() const -> std::filesystem::path {
  auto& filesystem_module = core::engine::get_module<filesystem::filesystem_module>();
  return filesystem_module.native_path_of(std::filesystem::path{"engine://dotnet"});
}

auto scripting_module::_load_game_assembly() -> void {
  const auto core_assembly_path = _dotnet_directory() / "Sbx.Core.dll";

  _script_compiler.compile_if_stale(_runtime, core_assembly_path);

  if (!_script_compiler.last_compile_succeeded()) {
    return;
  }

  if (_has_game_assembly) {
    _runtime.unload_assembly_load_context(_game_context);
    _has_game_assembly = false;

    _core_assembly.reload_types();
  }

  _game_context = _runtime.create_assembly_load_context("GameScripts");

  if (std::filesystem::exists(_script_compiler.output_path())) {
    const auto& output_path = _script_compiler.output_path();

    _game_assembly = _game_context.load_assembly(output_path.string());
    _has_game_assembly = true;
  }
}

auto scripting_module::_exception_callback(std::string_view message) -> void {
  // Never throw here: this runs synchronously inside a native function pointer that managed code
  // (Sbx.Managed's Host.HandleException, itself inside a C# catch block) calls directly -- a C++
  // exception would have to unwind back out through that CLR-JIT-compiled frame, which the .NET
  // native hosting interop does not support and will crash the process instead of surfacing the
  // error. Log and return; a script fault is reported this way, not by throwing.
  utility::logger<"scripting">::error("Script runtime error: {}", message);
}

} // namespace sbx::scripting
