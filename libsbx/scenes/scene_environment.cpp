// SPDX-License-Identifier: MIT
#include <libsbx/scenes/scene_environment.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/transform.hpp>

namespace sbx::scenes {

static auto _aspect_from(const math::vector2u& size) -> std::float_t {
  return size.y() == 0u ? 1.0f : static_cast<std::float_t>(size.x()) / static_cast<std::float_t>(size.y());
}

scene_environment::scene_environment(scene_graph& graph, const scenes::node camera, const math::vector3& light_direction, const math::color& light_color)
: _graph{graph},
  _camera{camera},
  _light{light_direction, light_color} { }

auto scene_environment::update_uniforms() -> void {
  auto& camera = _graph.get_component<scenes::camera>(_camera);

  const auto aspect = _aspect_from(_render_target_size);
  const auto projection = camera.projection(aspect);

  _uniform_handler.push("projection", projection);
  _uniform_handler.push("inverse_projection", math::matrix4x4::inverted(projection));

  auto inverse_view = _graph.world_transform(_camera);
  const auto view = math::matrix4x4::inverted(inverse_view);

  inverse_view[3] = math::vector4{0.0f, 0.0f, 0.0f, 1.0f};

  _uniform_handler.push("view", view);
  _uniform_handler.push("inverse_view", inverse_view);

  _view_projection = projection * view;

  _uniform_handler.push("viewport", _render_target_size);

  _uniform_handler.push("camera_position", _graph.world_position(_camera));
  _uniform_handler.push("camera_near", camera.near_plane());
  _uniform_handler.push("camera_far", camera.far_plane());
  _uniform_handler.push("camera_fov_radians", camera.field_of_view().to_radians().value());

  const auto camera_rotation = math::matrix_cast<math::matrix4x4>(_graph.world_rotation(_camera));
  const auto camera_right = math::vector3{camera_rotation[0]};
  const auto camera_up = math::vector3{camera_rotation[1]};

  _uniform_handler.push("camera_right", camera_right);
  _uniform_handler.push("camera_up", camera_up);

  _cached_csm = _build_csm(aspect);

  _uniform_handler.push("light_spaces", _cached_csm.light_spaces);
  _uniform_handler.push("cascade_splits", _cached_csm.cascade_splits);

  _uniform_handler.push("light_direction", math::vector3::normalized(_light.direction()));
  _uniform_handler.push("light_color", _light.color());

  _uniform_handler.push("time", core::engine::time().value());
  _uniform_handler.push("wind_direction", math::vector2{1.0f, 0.0f});
  _uniform_handler.push("wind_strength", std::float_t{0.5f});
}

auto scene_environment::light_space() -> math::matrix4x4 {
  const auto& camera = _graph.get_component<scenes::camera>(_camera);

  const auto camera_view = math::matrix4x4::inverted(_graph.world_transform(_camera));
  const auto camera_projection = camera.projection(_aspect_from(_render_target_size), 0.1f, 1000.0f);

  const auto inverse_view_projection = math::matrix4x4::inverted(camera_projection * camera_view);

  static constexpr auto frustum_corners_clip = std::array<math::vector4, 8u>{
    math::vector4{-1.0f, -1.0f, 0.0f, 1.0f},
    math::vector4{ 1.0f, -1.0f, 0.0f, 1.0f},
    math::vector4{ 1.0f,  1.0f, 0.0f, 1.0f},
    math::vector4{-1.0f,  1.0f, 0.0f, 1.0f},
    math::vector4{-1.0f, -1.0f, 1.0f, 1.0f},
    math::vector4{ 1.0f, -1.0f, 1.0f, 1.0f},
    math::vector4{ 1.0f,  1.0f, 1.0f, 1.0f},
    math::vector4{-1.0f,  1.0f, 1.0f, 1.0f}
  };

  auto frustum_corners_world = std::array<math::vector3, 8u>{};

  for (auto i = 0u; i < 8u; i++) {
    auto corner_world = inverse_view_projection * frustum_corners_clip[i];

    corner_world /= corner_world.w();
    frustum_corners_world[i] = math::vector3{corner_world};
  }

  auto center = math::vector3::zero;

  for (const auto& corner : frustum_corners_world) {
    center += corner;
  }

  center /= 8.0f;

  const auto light_dir = math::vector3::normalized(_light.direction());

  auto up = math::vector3::up;

  if (std::abs(math::vector3::dot(light_dir, up)) > 0.99f) {
    up = math::vector3{1.0f, 0.0f, 0.0f};
  }

  const auto light_view_initial = math::matrix4x4::look_at(center, center + light_dir, up);

  auto min_z = std::numeric_limits<std::float_t>::max();
  auto max_z = std::numeric_limits<std::float_t>::lowest();

  for (const auto& corner : frustum_corners_world) {
    const auto p = light_view_initial * math::vector4{corner, 1.0f};

    min_z = std::min(min_z, p.z());
    max_z = std::max(max_z, p.z());
  }

  static constexpr auto z_caster_padding = 50.0f;
  const auto pull_back = std::abs(min_z) + z_caster_padding;

  const auto light_position = center - light_dir * pull_back;

  const auto light_view = math::matrix4x4::look_at(light_position, center, up);

  auto min_bounds = math::vector3{std::numeric_limits<std::float_t>::max()};
  auto max_bounds = math::vector3{std::numeric_limits<std::float_t>::lowest()};

  for (const auto& corner : frustum_corners_world) {
    const auto corner_light_space = light_view * math::vector4{corner, 1.0f};
    const auto p = math::vector3{corner_light_space};

    min_bounds = math::vector3::min(min_bounds, p);
    max_bounds = math::vector3::max(max_bounds, p);
  }

  const auto z_range = max_bounds.z() - min_bounds.z();
  const auto z_padding = std::max(z_range * 2.0f, 50.0f);

  auto near_plane = -(max_bounds.z() + z_padding);
  auto far_plane  = -(min_bounds.z() - z_padding);

  if (near_plane > far_plane) {
    std::swap(near_plane, far_plane);
  }

  static constexpr auto xy_padding = 10.0f;

  const auto light_projection = math::matrix4x4::orthographic(
    min_bounds.x() - xy_padding, max_bounds.x() + xy_padding,
    min_bounds.y() - xy_padding, max_bounds.y() + xy_padding,
    near_plane, far_plane
  );

  return light_projection * light_view;
}

auto scene_environment::screen_point_to_ray(const math::vector2& position) -> math::ray {
  const auto& camera = _graph.get_component<scenes::camera>(_camera);

  const auto aspect = _aspect_from(_render_target_size);

  const auto world = _graph.world_transform(_camera);
  const auto view = math::matrix4x4::inverted(world);
  const auto projection = camera.projection(aspect);

  const auto inverse_view_projection = math::matrix4x4::inverted(projection * view);

  const auto px = position.x() + 0.5f;
  const auto py = position.y() + 0.5f;

  const auto x = (2.0f * px) / static_cast<std::float_t>(_render_target_size.x()) - 1.0f;
  const auto y = (2.0f * py) / static_cast<std::float_t>(_render_target_size.y()) - 1.0f;

  const auto near_clip = math::vector4{x, y, 0.0f, 1.0f};
  const auto far_clip  = math::vector4{x, y, 1.0f, 1.0f};

  auto near_world = inverse_view_projection * near_clip;
  near_world /= near_world.w();

  auto far_world = inverse_view_projection * far_clip;
  far_world /= far_world.w();

  const auto origin = math::vector3{near_world.x(), near_world.y(), near_world.z()};
  const auto direction = math::vector3::normalized(math::vector3{far_world.x(), far_world.y(), far_world.z()} - origin);

  return math::ray{origin, direction};
}

auto scene_environment::_compute_csm_splits(const std::float_t near_plane, const std::float_t far_plane, const std::float_t lambda) -> std::array<std::float_t, csm_cascade_count> {
  auto splits = std::array<std::float_t, csm_cascade_count>{};

  for (auto i = 0u; i < csm_cascade_count; ++i) {
    const auto p = static_cast<std::float_t>(i + 1u) / static_cast<std::float_t>(csm_cascade_count);

    const auto log_split = near_plane * std::pow(far_plane / near_plane, p);
    const auto uni_split = near_plane + (far_plane - near_plane) * p;

    splits[i] = _lerp_float(uni_split, log_split, lambda);
  }

  return splits;
}

auto scene_environment::_build_light_space_for_slice(const scenes::camera& camera, std::float_t aspect_ratio, const math::matrix4x4& camera_world, const math::vector3& light_direction, const std::float_t slice_near, const std::float_t slice_far, const std::uint32_t shadow_resolution) -> math::matrix4x4 {
  const auto fov_y = camera.field_of_view().to_radians().value();
  const auto tan_half_y = std::tan(fov_y * 0.5f);
  const auto tan_half_x = tan_half_y * aspect_ratio;

  const auto half_length = (slice_far - slice_near) * 0.5f;
  const auto tan_sq = tan_half_x * tan_half_x + tan_half_y * tan_half_y;
  const auto radius = std::sqrt(half_length * half_length + slice_far * slice_far * tan_sq);

  const auto camera_position = math::vector3{camera_world[3]};
  const auto camera_forward = -math::vector3{camera_world[2]};
  const auto center_world = camera_position + camera_forward * ((slice_near + slice_far) * 0.5f);

  const auto light_dir = math::vector3::normalized(light_direction);

  auto up = math::vector3::up;

  if (std::abs(math::vector3::dot(light_dir, up)) > 0.99f) {
    up = math::vector3{1.0f, 0.0f, 0.0f};
  }

  static constexpr auto z_caster_padding = 100.0f;
  const auto light_position = center_world - light_dir * (radius + z_caster_padding);

  const auto light_view = math::matrix4x4::look_at(light_position, center_world, up);

  const auto near_plane = 0.0f;
  const auto far_plane = 2.0f * radius + z_caster_padding;

  const auto light_projection = math::matrix4x4::orthographic(
    -radius, radius,
    -radius, radius,
    near_plane, far_plane
  );

  auto shadow_matrix = light_projection * light_view;

  const auto resolution = static_cast<std::float_t>(shadow_resolution);
  auto shadow_origin = shadow_matrix * math::vector4{0.0f, 0.0f, 0.0f, 1.0f};
  shadow_origin *= (resolution * 0.5f);

  const auto rounded_x = std::round(shadow_origin.x());
  const auto rounded_y = std::round(shadow_origin.y());

  const auto offset_x = (rounded_x - shadow_origin.x()) * (2.0f / resolution);
  const auto offset_y = (rounded_y - shadow_origin.y()) * (2.0f / resolution);

  shadow_matrix[3][0] += offset_x;
  shadow_matrix[3][1] += offset_y;

  return shadow_matrix;
}

auto scene_environment::_build_csm(std::float_t aspect_ratio) -> csm_data {
  const auto& camera = _graph.get_component<scenes::camera>(_camera);
  const auto camera_world = _graph.world_transform(_camera);

  static constexpr auto csm_near = 0.5f;
  static constexpr auto csm_far = 300.0f;
  static constexpr auto lambda = 0.85f;
  static constexpr auto shadow_resolutions = std::array<std::uint32_t, csm_cascade_count>{2048u, 2048u, 1024u, 512u};

  const auto effective_near = std::max(camera.near_plane(), csm_near);
  const auto effective_far = std::min(camera.far_plane(), csm_far);

  const auto splits = _compute_csm_splits(effective_near, effective_far, lambda);

  auto result = csm_data{};

  auto slice_near = effective_near;

  for (auto i = 0u; i < csm_cascade_count; ++i) {
    const auto slice_far = splits[i];

    result.light_spaces[i] = _build_light_space_for_slice(camera, aspect_ratio, camera_world, _light.direction(), slice_near, slice_far, shadow_resolutions[i]);

    slice_near = slice_far;
  }

  result.cascade_splits = math::vector4{splits[0], splits[1], splits[2], effective_far};

  return result;
}

} // namespace sbx::scenes
