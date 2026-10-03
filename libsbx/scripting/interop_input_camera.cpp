// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/interop.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

namespace sbx::scripting {

auto interop::input_is_key_pressed(platform::key key) -> managed::bool32 { 
  return platform::input::is_key_pressed(key); 
}

auto interop::input_is_key_down(platform::key key) -> managed::bool32 { 
  return platform::input::is_key_down(key); 
}

auto interop::input_is_key_released(platform::key key) -> managed::bool32 { 
  return platform::input::is_key_released(key); 
}

auto interop::input_is_mouse_button_pressed(platform::mouse_button mouse_button) -> managed::bool32 { 
  return platform::input::is_mouse_button_pressed(mouse_button); 
}

auto interop::input_is_mouse_button_down(platform::mouse_button mouse_button) -> managed::bool32 { 
  return platform::input::is_mouse_button_down(mouse_button); 
}

auto interop::input_is_mouse_button_released(platform::mouse_button mouse_button) -> managed::bool32 { 
  return platform::input::is_mouse_button_released(mouse_button); 
}

auto interop::input_mouse_position(math::vector2* position) -> void {
  *position = platform::input::mouse_position();
}

auto interop::input_scroll_delta(math::vector2* scroll_delta) -> void {
  *scroll_delta = platform::input::scroll_delta();
}

auto interop::camera_get_viewport(math::vector2* viewport) -> void {
  if (!viewport) {
    utility::logger<"scripting">::error("Attempting to get null viewport of camera");

    return;
  }

  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();
  const auto extent = scene_renderer_module.target_extent();

  *viewport = math::vector2{static_cast<std::float_t>(extent.x()), static_cast<std::float_t>(extent.y())};
}

// Viewport offset within the window: zero standalone, nonzero in the editor's docked game view. Mouse input is window space, UI is viewport space.
auto interop::camera_get_viewport_offset(math::vector2* offset) -> void {
  if (!offset) {
    utility::logger<"scripting">::error("Attempting to get null viewport offset of camera");

    return;
  }

  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();

  *offset = scene_renderer_module.viewport_offset();
}

auto interop::render_settings_set_wireframe_enabled(bool enabled) -> void {
  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();

  scene_renderer_module.set_wireframe_enabled(enabled);
}

auto interop::render_settings_get_wireframe_enabled() -> managed::bool32 {
  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();

  return scene_renderer_module.wireframe_enabled();
}

auto interop::camera_screen_point_to_ray(math::ray* ray, math::vector2* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call screen_point_to_ray with no active camera");

    return;
  }

  if (!ray || !position) {
    utility::logger<"scripting">::error("Attempting to call screen_point_to_ray with null ray/position of node '{}'", node.name());

    return;
  }

  const auto& camera = node.get_component<scenes::camera>();

  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();
  const auto extent = scene_renderer_module.target_extent();
  const auto offset = scene_renderer_module.viewport_offset();

  const auto aspect = (extent.y() > 0u) ? (static_cast<std::float_t>(extent.x()) / static_cast<std::float_t>(extent.y())) : 1.0f;

  const auto view = math::matrix4x4::inverted(node.world_matrix());
  const auto projection = math::matrix4x4::perspective(math::degree{camera.fov_degrees}, aspect, camera.near_plane, camera.far_plane);
  const auto inverse_view_projection = math::matrix4x4::inverted(projection * view);

  const auto ndc_x = (extent.x() > 0u) ? (((position->x() - offset.x()) / static_cast<std::float_t>(extent.x())) * 2.0f - 1.0f) : 0.0f;
  const auto ndc_y = (extent.y() > 0u) ? (((position->y() - offset.y()) / static_cast<std::float_t>(extent.y())) * 2.0f - 1.0f) : 0.0f;

  const auto unproject = [&inverse_view_projection, ndc_x, ndc_y](std::float_t ndc_z) -> math::vector3 {
    const auto point = inverse_view_projection * math::vector4{ndc_x, ndc_y, ndc_z, 1.0f};
    return math::vector3{point.x(), point.y(), point.z()} / point.w();
  };

  const auto near_point = unproject(0.0f);
  const auto far_point = unproject(1.0f);

  *ray = math::ray{near_point, far_point - near_point};
}

auto interop::camera_world_to_screen_point(math::vector3* world_position, math::vector2* out_position) -> managed::bool32 {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call world_to_screen_point with no active camera");

    return false;
  }

  if (!world_position || !out_position) {
    utility::logger<"scripting">::error("Attempting to call world_to_screen_point with null world/screen position of node '{}'", node.name());

    return false;
  }

  const auto& camera = node.get_component<scenes::camera>();

  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();
  const auto extent = scene_renderer_module.target_extent();
  const auto offset = scene_renderer_module.viewport_offset();

  const auto aspect = (extent.y() > 0u) ? (static_cast<std::float_t>(extent.x()) / static_cast<std::float_t>(extent.y())) : 1.0f;

  const auto view = math::matrix4x4::inverted(node.world_matrix());
  const auto projection = math::matrix4x4::perspective(math::degree{camera.fov_degrees}, aspect, camera.near_plane, camera.far_plane);

  const auto clip = projection * view * math::vector4{*world_position, 1.0f};

  // At or behind the camera's eye plane: no screen point.
  if (clip.w() <= 0.0f) {
    return false;
  }

  const auto ndc_x = clip.x() / clip.w();
  const auto ndc_y = clip.y() / clip.w();

  out_position->x() = (ndc_x * 0.5f + 0.5f) * static_cast<std::float_t>(extent.x()) + offset.x();
  out_position->y() = (ndc_y * 0.5f + 0.5f) * static_cast<std::float_t>(extent.y()) + offset.y();

  return true;
}

auto interop::camera_main_get_position(math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get position with no active camera");

    return;
  }

  *position = node.transform().position;
}

auto interop::camera_main_set_position(math::vector3* position) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set position with no active camera");

    return;
  }

  if (!position) {
    utility::logger<"scripting">::error("Attempting to set null position of camera node '{}'", node.name());

    return;
  }

  node.transform().position = *position;
}

auto interop::camera_main_get_rotation(math::quaternion* rotation) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get rotation with no active camera");

    return;
  }

  *rotation = node.transform().rotation;
}

auto interop::camera_main_set_rotation(math::quaternion* rotation) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set rotation with no active camera");

    return;
  }

  if (!rotation) {
    utility::logger<"scripting">::error("Attempting to set null rotation of camera node '{}'", node.name());

    return;
  }

  node.transform().rotation = *rotation;
}

auto interop::camera_main_get_forward(math::vector3* forward) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get forward with no active camera");

    return;
  }

  *forward = node.transform().forward();
}

auto interop::camera_main_get_right(math::vector3* right) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get right with no active camera");

    return;
  }

  *right = node.transform().right();
}

auto interop::camera_main_get_up(math::vector3* up) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.active_camera();

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get up with no active camera");

    return;
  }

  *up = node.transform().up();
}

auto interop::camera_get_fov_degrees(std::uint64_t uuid, std::float_t* fov_degrees) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !fov_degrees) {
    utility::logger<"scripting">::error("Attempting to get fov_degrees of invalid node");

    return;
  }

  *fov_degrees = node.get_component<scenes::camera>().fov_degrees;
}

auto interop::camera_set_fov_degrees(std::uint64_t uuid, std::float_t fov_degrees) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set fov_degrees of invalid node");

    return;
  }

  node.get_component<scenes::camera>().fov_degrees = fov_degrees;
}

auto interop::camera_get_near_plane(std::uint64_t uuid, std::float_t* near_plane) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !near_plane) {
    utility::logger<"scripting">::error("Attempting to get near_plane of invalid node");

    return;
  }

  *near_plane = node.get_component<scenes::camera>().near_plane;
}

auto interop::camera_set_near_plane(std::uint64_t uuid, std::float_t near_plane) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set near_plane of invalid node");

    return;
  }

  node.get_component<scenes::camera>().near_plane = near_plane;
}

auto interop::camera_get_far_plane(std::uint64_t uuid, std::float_t* far_plane) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !far_plane) {
    utility::logger<"scripting">::error("Attempting to get far_plane of invalid node");

    return;
  }

  *far_plane = node.get_component<scenes::camera>().far_plane;
}

auto interop::camera_set_far_plane(std::uint64_t uuid, std::float_t far_plane) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set far_plane of invalid node");

    return;
  }

  node.get_component<scenes::camera>().far_plane = far_plane;
}

auto interop::camera_get_exposure(std::uint64_t uuid, std::float_t* exposure) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !exposure) {
    utility::logger<"scripting">::error("Attempting to get exposure of invalid node");

    return;
  }

  *exposure = node.get_component<scenes::camera>().post_process.exposure;
}

auto interop::camera_set_exposure(std::uint64_t uuid, std::float_t exposure) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set exposure of invalid node");

    return;
  }

  node.get_component<scenes::camera>().post_process.exposure = exposure;
}

auto interop::camera_get_post_process(std::uint64_t uuid, post_process_data* out_value) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_value || !node.has_component<scenes::camera>()) {
    utility::logger<"scripting">::error("Attempting to get post processing of invalid camera node");

    return;
  }

  const auto& post = node.get_component<scenes::camera>().post_process;

  const auto& dof = post.depth_of_field;
  const auto& grading = post.color_grading;

  *out_value = post_process_data{
    .exposure = post.exposure,
    .bloom_enabled = post.bloom.enabled ? 1u : 0u,
    .bloom_intensity = post.bloom.intensity,
    .bloom_threshold = post.bloom.threshold,
    .bloom_knee = post.bloom.knee,
    .dof_enabled = dof.enabled ? 1u : 0u,
    .dof_mode = static_cast<std::uint32_t>(dof.mode),
    .dof_focus_distance = dof.focus_distance,
    .dof_focus_range = dof.focus_range,
    .dof_band_center = dof.band_center,
    .dof_band_height = dof.band_height,
    .dof_max_blur = dof.max_blur,
    .dof_samples = dof.samples,
    .lut = grading.lut.is_valid() ? grading.lut->id().value() : 0u,
    .lut_contribution = grading.lut_contribution,
    .contrast = grading.contrast,
    .saturation = grading.saturation,
    .fog_enabled = post.fog.enabled ? 1u : 0u,
    .fog_color = post.fog.color,
    .fog_density = post.fog.density,
    .fog_start = post.fog.start,
    .fog_height_falloff = post.fog.height_falloff,
    .fog_base_height = post.fog.base_height,
    .fog_max_opacity = post.fog.max_opacity,
    .fog_affects_sky = post.fog.affects_sky ? 1u : 0u,
    .ao_enabled = post.ambient_occlusion.enabled ? 1u : 0u,
    .ao_radius = post.ambient_occlusion.radius,
    .ao_intensity = post.ambient_occlusion.intensity,
    .ao_samples = post.ambient_occlusion.samples
  };
}

auto interop::camera_set_post_process(std::uint64_t uuid, const post_process_data* value) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !value || !node.has_component<scenes::camera>()) {
    utility::logger<"scripting">::error("Attempting to set post processing of invalid camera node");

    return;
  }

  auto& post = node.get_component<scenes::camera>().post_process;

  post.exposure = value->exposure;
  post.bloom.enabled = value->bloom_enabled != 0u;
  post.bloom.intensity = value->bloom_intensity;
  post.bloom.threshold = value->bloom_threshold;
  post.bloom.knee = value->bloom_knee;

  auto& dof = post.depth_of_field;
  dof.enabled = value->dof_enabled != 0u;
  dof.mode = value->dof_mode == 1u ? scenes::post_process_settings::depth_of_field_settings::focus_mode::screen_band : scenes::post_process_settings::depth_of_field_settings::focus_mode::distance;
  dof.focus_distance = value->dof_focus_distance;
  dof.focus_range = value->dof_focus_range;
  dof.band_center = value->dof_band_center;
  dof.band_height = value->dof_band_height;
  dof.max_blur = value->dof_max_blur;
  dof.samples = value->dof_samples;

  auto& grading = post.color_grading;
  const auto current = grading.lut.is_valid() ? grading.lut->id().value() : 0u;

  // Scripts write the whole struct back on every edit, so only reload the lookup table when it changed.
  const auto current_handle = grading.lut.is_valid() ? grading.lut->handle().value() : 0u;

  if (value->lut != current && value->lut != current_handle) {
    auto& assets_module = core::engine::get_module<assets::assets_module>();
    auto lut = value->lut != 0u ? assets_module.find_texture(math::uuid::from_value(value->lut)) : assets::texture2d_handle{};

    if (value->lut != 0u && !lut.is_valid()) {
      lut = assets_module.load_texture(math::uuid::from_value(value->lut), graphics::format::r8g8b8a8_unorm);
    }

    grading.lut = lut;
  }

  grading.lut_contribution = value->lut_contribution;
  grading.contrast = value->contrast;
  grading.saturation = value->saturation;

  auto& fog = post.fog;
  fog.enabled = value->fog_enabled != 0u;
  fog.color = value->fog_color;
  fog.density = value->fog_density;
  fog.start = value->fog_start;
  fog.height_falloff = value->fog_height_falloff;
  fog.base_height = value->fog_base_height;
  fog.max_opacity = value->fog_max_opacity;
  fog.affects_sky = value->fog_affects_sky != 0u;

  auto& ao = post.ambient_occlusion;
  ao.enabled = value->ao_enabled != 0u;
  ao.radius = value->ao_radius;
  ao.intensity = value->ao_intensity;
  ao.samples = value->ao_samples;
}

auto interop::time_delta_time(std::float_t* delta_time) -> void {
  if (!delta_time) {
    utility::logger<"scripting">::error("Attempting to set null delta_time");

    return;
  }

  *delta_time = core::engine::delta_time().value();
}

} // namespace sbx::scripting
