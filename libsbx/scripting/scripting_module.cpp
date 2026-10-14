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
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_GetLayer", reinterpret_cast<void*>(&interop::node_get_layer));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_SetLayer", reinterpret_cast<void*>(&interop::node_set_layer));

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
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_GetCollisionDetection", reinterpret_cast<void*>(&interop::rigidbody_get_collision_detection));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_SetCollisionDetection", reinterpret_cast<void*>(&interop::rigidbody_set_collision_detection));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_AddForce", reinterpret_cast<void*>(&interop::rigidbody_add_force));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Rigidbody_AddTorque", reinterpret_cast<void*>(&interop::rigidbody_add_torque));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_FindByName", reinterpret_cast<void*>(&interop::node_find_by_name));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_Create", reinterpret_cast<void*>(&interop::node_create));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_InstantiatePrefab", reinterpret_cast<void*>(&interop::node_instantiate_prefab));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_Destroy", reinterpret_cast<void*>(&interop::node_destroy));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_SetParent", reinterpret_cast<void*>(&interop::node_set_parent));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_SetActive", reinterpret_cast<void*>(&interop::node_set_active));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_GetIsActive", reinterpret_cast<void*>(&interop::node_get_is_active));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_Save", reinterpret_cast<void*>(&interop::scene_save));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_New", reinterpret_cast<void*>(&interop::scene_new));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_LoadByName", reinterpret_cast<void*>(&interop::scene_load_by_name));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_LoadByIndex", reinterpret_cast<void*>(&interop::scene_load_by_index));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_CountInBuildSettings", reinterpret_cast<void*>(&interop::scene_count_in_build_settings));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_LoadAsyncByName", reinterpret_cast<void*>(&interop::scene_load_async_by_name));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_LoadAsyncByIndex", reinterpret_cast<void*>(&interop::scene_load_async_by_index));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_GetOperationProgress", reinterpret_cast<void*>(&interop::scene_get_operation_progress));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_GetAllowActivation", reinterpret_cast<void*>(&interop::scene_get_allow_activation));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_SetAllowActivation", reinterpret_cast<void*>(&interop::scene_set_allow_activation));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_Unload", reinterpret_cast<void*>(&interop::scene_unload));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_IsOperationDone", reinterpret_cast<void*>(&interop::scene_is_operation_done));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_GetCount", reinterpret_cast<void*>(&interop::scene_get_count));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_GetAt", reinterpret_cast<void*>(&interop::scene_get_at));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_GetActive", reinterpret_cast<void*>(&interop::scene_get_active));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_SetActive", reinterpret_cast<void*>(&interop::scene_set_active));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_FindByName", reinterpret_cast<void*>(&interop::scene_find_by_name));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_FindByPath", reinterpret_cast<void*>(&interop::scene_find_by_path));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_FindByBuildIndex", reinterpret_cast<void*>(&interop::scene_find_by_build_index));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_GetName", reinterpret_cast<void*>(&interop::scene_get_name));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_GetPath", reinterpret_cast<void*>(&interop::scene_get_path));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_GetBuildIndex", reinterpret_cast<void*>(&interop::scene_get_build_index));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_IsLoaded", reinterpret_cast<void*>(&interop::scene_is_loaded));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_GetRootNodes", reinterpret_cast<void*>(&interop::scene_get_root_nodes));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Scene_MoveNode", reinterpret_cast<void*>(&interop::scene_move_node));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Node_DontDestroyOnLoad", reinterpret_cast<void*>(&interop::node_dont_destroy_on_load));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_Load", reinterpret_cast<void*>(&interop::particle_effect_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_Play", reinterpret_cast<void*>(&interop::particle_effect_play));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_Pause", reinterpret_cast<void*>(&interop::particle_effect_pause));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_Stop", reinterpret_cast<void*>(&interop::particle_effect_stop));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_GetLoop", reinterpret_cast<void*>(&interop::particle_effect_get_loop));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_SetLoop", reinterpret_cast<void*>(&interop::particle_effect_set_loop));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ParticleEffect_GetIsPlaying", reinterpret_cast<void*>(&interop::particle_effect_get_is_playing));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioClip_Load", reinterpret_cast<void*>(&interop::audio_clip_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioClip_IsLoaded", reinterpret_cast<void*>(&interop::audio_clip_is_loaded));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioClip_GetLength", reinterpret_cast<void*>(&interop::audio_clip_get_length));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioClip_GetChannels", reinterpret_cast<void*>(&interop::audio_clip_get_channels));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioClip_GetFrequency", reinterpret_cast<void*>(&interop::audio_clip_get_frequency));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_GetSettings", reinterpret_cast<void*>(&interop::audio_source_get_settings));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_SetSettings", reinterpret_cast<void*>(&interop::audio_source_set_settings));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_GetClip", reinterpret_cast<void*>(&interop::audio_source_get_clip));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_SetClip", reinterpret_cast<void*>(&interop::audio_source_set_clip));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_GetOutputGroup", reinterpret_cast<void*>(&interop::audio_source_get_output_group));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_SetOutputGroup", reinterpret_cast<void*>(&interop::audio_source_set_output_group));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_Play", reinterpret_cast<void*>(&interop::audio_source_play));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_Pause", reinterpret_cast<void*>(&interop::audio_source_pause));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_UnPause", reinterpret_cast<void*>(&interop::audio_source_unpause));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_Stop", reinterpret_cast<void*>(&interop::audio_source_stop));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_GetIsPlaying", reinterpret_cast<void*>(&interop::audio_source_get_is_playing));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_GetTime", reinterpret_cast<void*>(&interop::audio_source_get_time));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_SetTime", reinterpret_cast<void*>(&interop::audio_source_set_time));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioSource_PlayOneShot", reinterpret_cast<void*>(&interop::audio_source_play_one_shot));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioListener_GetVolume", reinterpret_cast<void*>(&interop::audio_listener_get_volume));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "AudioListener_SetVolume", reinterpret_cast<void*>(&interop::audio_listener_set_volume));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Audio_PlayClipAtPoint", reinterpret_cast<void*>(&interop::audio_play_clip_at_point));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Audio_SetMixerGroupVolume", reinterpret_cast<void*>(&interop::audio_set_mixer_group_volume));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Audio_GetMixerGroupVolume", reinterpret_cast<void*>(&interop::audio_get_mixer_group_volume));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Audio_ResetMixerGroupVolume", reinterpret_cast<void*>(&interop::audio_reset_mixer_group_volume));

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
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RenderSettings_GetMsaaSamples", reinterpret_cast<void*>(&interop::render_settings_get_msaa_samples));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "RenderSettings_SetMsaaSamples", reinterpret_cast<void*>(&interop::render_settings_set_msaa_samples));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetFovDegrees", reinterpret_cast<void*>(&interop::camera_get_fov_degrees));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetFovDegrees", reinterpret_cast<void*>(&interop::camera_set_fov_degrees));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetNearPlane", reinterpret_cast<void*>(&interop::camera_get_near_plane));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetNearPlane", reinterpret_cast<void*>(&interop::camera_set_near_plane));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetFarPlane", reinterpret_cast<void*>(&interop::camera_get_far_plane));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetFarPlane", reinterpret_cast<void*>(&interop::camera_set_far_plane));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetExposure", reinterpret_cast<void*>(&interop::camera_get_exposure));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetExposure", reinterpret_cast<void*>(&interop::camera_set_exposure));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_GetPostProcess", reinterpret_cast<void*>(&interop::camera_get_post_process));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Camera_SetPostProcess", reinterpret_cast<void*>(&interop::camera_set_post_process));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Time_DeltaTime", reinterpret_cast<void*>(&interop::time_delta_time));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Time_FixedDeltaTime", reinterpret_cast<void*>(&interop::time_fixed_delta_time));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Physics_Raycast", reinterpret_cast<void*>(&interop::physics_raycast));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Physics_OverlapSphere", reinterpret_cast<void*>(&interop::physics_overlap_sphere));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Physics_ShapeCast", reinterpret_cast<void*>(&interop::physics_shape_cast));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Physics_OverlapShape", reinterpret_cast<void*>(&interop::physics_overlap_shape));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetSettings", reinterpret_cast<void*>(&interop::character_controller_get_settings));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_SetSettings", reinterpret_cast<void*>(&interop::character_controller_set_settings));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetIsGrounded", reinterpret_cast<void*>(&interop::character_controller_get_is_grounded));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetFlags", reinterpret_cast<void*>(&interop::character_controller_get_flags));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_GetVelocity", reinterpret_cast<void*>(&interop::character_controller_get_velocity));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_Move", reinterpret_cast<void*>(&interop::character_controller_move));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "CharacterController_SimpleMove", reinterpret_cast<void*>(&interop::character_controller_simple_move));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "DebugDraw_Line", reinterpret_cast<void*>(&interop::debug_draw_line));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "DebugDraw_WireSphere", reinterpret_cast<void*>(&interop::debug_draw_wire_sphere));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "DebugDraw_WireBox", reinterpret_cast<void*>(&interop::debug_draw_wire_box));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "DebugDraw_WireCircle", reinterpret_cast<void*>(&interop::debug_draw_wire_circle));

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
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Terrain_Load", reinterpret_cast<void*>(&interop::terrain_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Terrain_SampleHeight", reinterpret_cast<void*>(&interop::terrain_sample_height));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Terrain_SampleNormal", reinterpret_cast<void*>(&interop::terrain_sample_normal));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Math_NoiseSimplex", reinterpret_cast<void*>(&interop::math_noise_simplex));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Math_NoiseFractal", reinterpret_cast<void*>(&interop::math_noise_fractal));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "MeshRenderer_SetGeometry", reinterpret_cast<void*>(&interop::mesh_renderer_set_geometry));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "MeshRenderer_SetMaterial", reinterpret_cast<void*>(&interop::mesh_renderer_set_material));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "InstancedMeshRenderer_SetGeometry", reinterpret_cast<void*>(&interop::instanced_mesh_renderer_set_geometry));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "InstancedMeshRenderer_SetMaterial", reinterpret_cast<void*>(&interop::instanced_mesh_renderer_set_material));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "InstancedMeshRenderer_SetInstances", reinterpret_cast<void*>(&interop::instanced_mesh_renderer_set_instances));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_Load", reinterpret_cast<void*>(&interop::material_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_CreateInstance", reinterpret_cast<void*>(&interop::material_create_instance));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_SetTexture", reinterpret_cast<void*>(&interop::material_set_texture));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_SetGenericParam", reinterpret_cast<void*>(&interop::material_set_generic_param));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_SetGenericTexture", reinterpret_cast<void*>(&interop::material_set_generic_texture));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_Release", reinterpret_cast<void*>(&interop::material_release));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_IsLoaded", reinterpret_cast<void*>(&interop::material_is_loaded));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Project_GetAssetsDirectory", reinterpret_cast<void*>(&interop::project_get_assets_directory));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_Load", reinterpret_cast<void*>(&interop::texture_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_CreateStorageImage", reinterpret_cast<void*>(&interop::texture_create_storage_image));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_ReadPixels", reinterpret_cast<void*>(&interop::texture_read_pixels));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_Release", reinterpret_cast<void*>(&interop::texture_release));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Debug_WritePng", reinterpret_cast<void*>(&interop::debug_write_png));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture_IsResident", reinterpret_cast<void*>(&interop::texture_is_resident));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture2DArray_Create", reinterpret_cast<void*>(&interop::texture2d_array_create));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture2DArray_IsResident", reinterpret_cast<void*>(&interop::texture2d_array_is_resident));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Texture2DArray_Release", reinterpret_cast<void*>(&interop::texture2d_array_release));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_SetGenericTextureArray", reinterpret_cast<void*>(&interop::material_set_generic_texture_array));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Font_Load", reinterpret_cast<void*>(&interop::font_load));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Font_IsResident", reinterpret_cast<void*>(&interop::font_is_resident));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Font_GetGlyph", reinterpret_cast<void*>(&interop::font_get_glyph));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Font_GetMetrics", reinterpret_cast<void*>(&interop::font_get_metrics));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "Material_SetGenericTextureFont", reinterpret_cast<void*>(&interop::material_set_generic_texture_font));

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
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIText_GetAlignment", reinterpret_cast<void*>(&interop::ui_text_get_alignment));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIText_SetAlignment", reinterpret_cast<void*>(&interop::ui_text_set_alignment));

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
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIScrollRect_SetContent", reinterpret_cast<void*>(&interop::ui_scroll_rect_set_content));

  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIMask_GetShowMaskGraphic", reinterpret_cast<void*>(&interop::ui_mask_get_show_mask_graphic));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "UIMask_SetShowMaskGraphic", reinterpret_cast<void*>(&interop::ui_mask_set_show_mask_graphic));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "LayoutGroup_Get", reinterpret_cast<void*>(&interop::layout_group_get));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "LayoutGroup_Set", reinterpret_cast<void*>(&interop::layout_group_set));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "LayoutElement_Get", reinterpret_cast<void*>(&interop::layout_element_get));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "LayoutElement_Set", reinterpret_cast<void*>(&interop::layout_element_set));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ContentSizeFitter_Get", reinterpret_cast<void*>(&interop::content_size_fitter_get));
  _core_assembly.add_internal_call("Sbx.Core.InternalCalls", "ContentSizeFitter_Set", reinterpret_cast<void*>(&interop::content_size_fitter_set));

  _register_managed_components();

  _core_assembly.upload_internal_calls();

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  scenes_module.on_scene_unloading().connect([this](std::uint32_t handle) { _destroy_scene_member_scripts(handle); });
  scenes_module.on_scene_loaded().connect([this](const scenes::scene_loaded_event& event) {
    // Before instantiating, so a scene saved before its script was renamed still starts the right class.
    resolve_renamed_scripts(core::engine::get_module<scenes::scenes_module>().active_scene());
    _instantiate_scene_member_scripts(event.handle);
    _broadcast("DispatchSceneLoaded", event.handle, static_cast<std::uint32_t>(event.mode));
  });
  scenes_module.on_scene_unloaded().connect([this](const scenes::loaded_scene& scene) { _broadcast("DispatchSceneUnloaded", scene.handle); });
  scenes_module.on_active_scene_changed().connect([this](const scenes::active_scene_changed_event& event) { _broadcast("DispatchActiveSceneChanged", event.previous, event.current); });

  auto& physics_module = core::engine::get_module<physics::physics_module>();

  physics_module.on_contact_began().connect([this](const physics::collision_event& event) { _dispatch_collision_event(event, true); });
  physics_module.on_contact_ended().connect([this](const physics::collision_event& event) { _dispatch_collision_event(event, false); });
  physics_module.on_controller_hit().connect([this](const physics::controller_hit& hit) { _dispatch_controller_hit(hit); });

  auto& canvas_module = core::engine::get_module<canvas::canvas_module>();

  canvas_module.on_button_clicked().connect([this](const scenes::node& node) { _dispatch_button_click(node); });

  // Queued geometry is swapped in here, where the render thread can't be using the mesh it replaces.
  core::engine::get_module<render::presentation_module>().on_render_idle().connect([]() { interop::apply_pending_geometry(); });
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

  // Like Unity, inactive nodes don't receive OnUpdate; OnCreate/OnDestroy still fire.
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
  // Create every instance before any OnCreate runs, so GetComponent in OnCreate can find siblings' scripts.
  for (auto&& [entity, list] : target.query<scenes::script_component>().each()) {
    auto node = target.node_of(entity);

    for (const auto& entry : list.scripts) {
      instantiate(node, entry.class_name);
    }
  }

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

  // Create every instance before any OnCreate runs, so GetComponent in OnCreate can find siblings' scripts.
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

  scripts.scripts.push_back(scenes::script_entry{.class_name = std::string{class_name}, .script = script_asset_of(class_name)});

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

  // Runs on every Inspector draw, so skip constructing a scratch instance when nothing is missing.
  const auto has_missing = std::ranges::any_of(type.get_fields(), [&](auto& field) {
    return field.get_accessibility() == managed::type_accessibility::public_access
        && script_field_type_of(field.get_type())
        && !already_seeded(std::string{field.get_name()});
  });

  if (!has_missing) {
    return;
  }

  // A throwaway instance just to read field defaults; assumes real setup happens in OnCreate, not the constructor.
  auto scratch = type.create_instance(node.get_component<scenes::id>().value());

  if (!scratch.is_valid()) {
    utility::logger<"scripting">::warn("Could not construct '{}' to read its script field defaults", entry.class_name);
    return;
  }

  for (auto& field : type.get_fields()) {
    if (field.get_accessibility() != managed::type_accessibility::public_access) {
      continue;
    }

    const auto field_type = script_field_type_of(field.get_type());

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
      case scenes::script_field_type::vector2: default_value.vector2_value = scratch.get_field_value<math::vector2>(field_name); break;
      case scenes::script_field_type::vector3: default_value.vector3_value = scratch.get_field_value<math::vector3>(field_name); break;
      case scenes::script_field_type::node:    default_value.node_value = math::uuid::from_value(scratch.get_field_value<std::uint64_t>(field_name)); break;
      case scenes::script_field_type::layer_mask: default_value.layer_mask_value = scratch.get_field_value<std::uint32_t>(field_name); break;
      case scenes::script_field_type::material: default_value.material_value = math::uuid::from_value(scratch.get_field_value<std::uint64_t>(field_name)); break;
      case scenes::script_field_type::color: default_value.color_value = scratch.get_field_value<math::color>(field_name); break;
      case scenes::script_field_type::enumeration: default_value.int_value = scratch.get_field_value<std::int32_t>(field_name); break;
      case scenes::script_field_type::texture: default_value.texture_value = math::uuid::from_value(scratch.get_field_value<std::uint64_t>(field_name)); break;
      case scenes::script_field_type::audio_clip: default_value.audio_clip_value = math::uuid::from_value(scratch.get_field_value<std::uint64_t>(field_name)); break;
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
      case scenes::script_field_type::vector2: instance.set_field_value(field.name, field.vector2_value); break;
      case scenes::script_field_type::vector3: instance.set_field_value(field.name, field.vector3_value); break;
      // Managed references cross as a raw uuid via INativeHandle.
      case scenes::script_field_type::node:    instance.set_field_value(field.name, field.node_value.value()); break;
      case scenes::script_field_type::layer_mask: instance.set_field_value(field.name, field.layer_mask_value); break;
      case scenes::script_field_type::material: instance.set_field_value(field.name, field.material_value.value()); break;
      case scenes::script_field_type::color: instance.set_field_value(field.name, field.color_value); break;
      // ponytail: enums cross as int32 -- a long-backed enum would read past it on the C# side; widen if one ever shows up.
      case scenes::script_field_type::enumeration: instance.set_field_value(field.name, field.int_value); break;
      case scenes::script_field_type::texture: instance.set_field_value(field.name, field.texture_value.value()); break;
      case scenes::script_field_type::audio_clip: instance.set_field_value(field.name, field.audio_clip_value.value()); break;
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

auto scripting_module::_dispatch_controller_hit(const physics::controller_hit& hit) -> void {
  auto controller = hit.controller;

  if (!controller.is_valid()) {
    return;
  }

  auto scripts = controller.try_get_component<scripting::scripts>();

  if (!scripts) {
    return;
  }

  const auto other_uuid = hit.other.is_valid() ? hit.other.get_component<scenes::id>().value() : std::uint64_t{0u};

  for (auto& instance : scripts->instances) {
    instance.invoke("DispatchControllerColliderHit", other_uuid, hit.point.x(), hit.point.y(), hit.point.z(), hit.normal.x(), hit.normal.y(), hit.normal.z(), hit.move_direction.x(), hit.move_direction.y(), hit.move_direction.z(), hit.move_length);
  }
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

auto scripting_module::_instantiate_scene_member_scripts(std::uint32_t handle) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  // Edit mode loads scenes too, and their scripts must not run.
  if (!scenes_module.is_simulating()) {
    return;
  }

  // Create every instance before any OnCreate runs, so GetComponent in OnCreate can find siblings' scripts.
  auto created = std::vector<managed::object>{};

  for (auto node : scenes_module.active_scene().members_of(handle)) {
    if (const auto* list = node.try_get_component<scenes::script_component>().get()) {
      for (const auto& entry : list->scripts) {
        created.push_back(instantiate(node, entry.class_name));
      }
    }
  }

  for (auto& instance : created) {
    instance.invoke("OnCreate");
  }
}

auto scripting_module::_destroy_scene_member_scripts(std::uint32_t handle) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  if (!scenes_module.is_simulating()) {
    return;
  }

  for (auto node : scenes_module.active_scene().members_of(handle)) {
    if (auto scripts = node.try_get_component<scripting::scripts>()) {
      for (auto& instance : scripts->instances) {
        instance.invoke("OnDestroy");
        instance.destroy();
      }

      scripts->instances.clear();
    }
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

  _resources.clear();
}

auto scripting_module::recompile_scripts() -> bool {
  _load_game_assembly();

  return resolve_renamed_scripts(core::engine::get_module<scenes::scenes_module>().active_scene());
}

// A class's name without its namespace, which is what its .cs file is named.
static auto short_class_name(std::string_view class_name) -> std::string_view {
  const auto dot = class_name.rfind('.');
  return dot == std::string_view::npos ? class_name : class_name.substr(dot + 1u);
}

auto scripting_module::script_asset_of(std::string_view class_name) const -> math::uuid {
  const auto name = short_class_name(class_name);

  for (const auto& [id, path] : core::engine::get_module<assets::assets_module>().assets_of(assets::asset_type::script)) {
    if (path.stem().string() == name) {
      return id;
    }
  }

  return math::uuid::nil();
}

auto scripting_module::resolve_renamed_scripts(scenes::scene& target) -> bool {
  if (!_has_game_assembly) {
    return false;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto exists = [this](const std::string& class_name) {
    return _game_assembly.get_type(class_name).get_type_id() != -1;
  };

  auto changed = false;

  for (auto&& [entity, component] : target.query<scenes::script_component>().each()) {
    for (auto& entry : component.scripts) {
      if (entry.script == math::uuid::nil() || exists(entry.class_name)) {
        continue;
      }

      const auto path = assets_module.path_of(entry.script);

      if (path.empty()) {
        continue; // the script was deleted: stays missing, like Unity's "Missing (Mono Script)"
      }

      // Keeps any namespace: only the class's own name follows the file.
      const auto short_name = short_class_name(entry.class_name);
      const auto renamed = entry.class_name.substr(0u, entry.class_name.size() - short_name.size()) + path.stem().string();

      if (renamed != entry.class_name && exists(renamed)) {
        utility::logger<"scripting">::info("Script '{}' was renamed to '{}'", entry.class_name, renamed);
        entry.class_name = renamed;
        changed = true;
      }
    }
  }

  return changed;
}

auto scripting_module::assign_missing_script_ids(scenes::scene& target, std::uint32_t handle) -> void {
  for (auto node : target.members_of(handle)) {
    if (auto component = node.try_get_component<scenes::script_component>()) {
      for (auto& entry : component->scripts) {
        if (entry.script == math::uuid::nil()) {
          entry.script = script_asset_of(entry.class_name);
        }
      }
    }
  }
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
    _resources.clear();
    _runtime.unload_assembly_load_context(_game_context);
    _has_game_assembly = false;

    _core_assembly.reload_types();

    // reload_types gives every Sbx.Core type a fresh id, so the component lookup tables must be rebuilt.
    _register_managed_components();
  }

  _game_context = _runtime.create_assembly_load_context("GameScripts");

  if (std::filesystem::exists(_script_compiler.output_path())) {
    const auto& output_path = _script_compiler.output_path();

    _game_assembly = _game_context.load_assembly(output_path.string());
    _has_game_assembly = true;
  }
}

auto scripting_module::_register_managed_components() -> void {
  interop::register_managed_component<scenes::tag>("Sbx.Core.Components.Tag", _core_assembly);
  interop::register_managed_component<scenes::local_transform>("Sbx.Core.Components.Transform", _core_assembly);
  interop::register_managed_component<scenes::animator>("Sbx.Core.Components.Animator", _core_assembly);
  interop::register_managed_component<scenes::particle_effect>("Sbx.Core.Components.ParticleEffect", _core_assembly);
  interop::register_managed_component<scenes::audio_source>("Sbx.Core.Components.AudioSource", _core_assembly);
  interop::register_managed_component<scenes::audio_listener>("Sbx.Core.Components.AudioListener", _core_assembly);
  interop::register_managed_component<scenes::camera>("Sbx.Core.Components.CameraSettings", _core_assembly);
  interop::register_managed_component<physics::rigidbody>("Sbx.Core.Physics.Rigidbody", _core_assembly);
  interop::register_managed_component<physics::nav_agent>("Sbx.Core.Physics.NavAgent", _core_assembly);
  interop::register_managed_component<physics::character_controller>("Sbx.Core.Physics.CharacterController", _core_assembly);
  interop::register_managed_component<scenes::mesh_renderer>("Sbx.Core.Components.MeshRenderer", _core_assembly);
  interop::register_managed_component<scenes::instanced_mesh_renderer>("Sbx.Core.Components.InstancedMeshRenderer", _core_assembly);
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
  interop::register_managed_component<canvas::horizontal_layout_group>("Sbx.Core.UI.HorizontalLayoutGroup", _core_assembly);
  interop::register_managed_component<canvas::vertical_layout_group>("Sbx.Core.UI.VerticalLayoutGroup", _core_assembly);
  interop::register_managed_component<canvas::layout_element>("Sbx.Core.UI.LayoutElement", _core_assembly);
  interop::register_managed_component<canvas::content_size_fitter>("Sbx.Core.UI.ContentSizeFitter", _core_assembly);
}

auto scripting_module::_exception_callback(std::string_view message) -> void {
  // Never throw: this is called from managed code, and a C++ exception can't unwind through the CLR frame.
  utility::logger<"scripting">::error("Script runtime error: {}", message);
}

} // namespace sbx::scripting
