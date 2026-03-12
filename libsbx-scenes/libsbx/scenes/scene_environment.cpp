// SPDX-License-Identifier: MIT
#include <libsbx/scenes/scene_environment.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/transform.hpp>

namespace sbx::scenes {

scene_environment::scene_environment(scene_graph& graph, const scenes::node camera, const math::vector3& light_direction, const math::color& light_color)
: _graph{graph},
  _camera{camera},
  _light{light_direction, light_color} { }

auto scene_environment::update_uniforms() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& camera = _graph.get_component<scenes::camera>(_camera);

  const auto& projection = camera.projection();

  _uniform_handler.push("projection", projection);
  _uniform_handler.push("inverse_projection", math::matrix4x4::inverted(projection));

  auto inverse_view = _graph.world_transform(_camera);

  const auto view = math::matrix4x4::inverted(inverse_view);

  inverse_view[3] = math::vector4{0.0f, 0.0f, 0.0f, 1.0f};

  _uniform_handler.push("view", view);
  _uniform_handler.push("inverse_view", inverse_view);

  _uniform_handler.push("viewport", graphics_module.viewport());

  _uniform_handler.push("camera_position", _graph.world_position(_camera));
  _uniform_handler.push("camera_near", camera.near_plane());
  _uniform_handler.push("camera_far", camera.far_plane());
  _uniform_handler.push("camera_fov_radians", camera.field_of_view().to_radians().value());

  const auto camera_rotation = math::matrix_cast<math::matrix4x4>(_graph.world_rotation(_camera));
  const auto camera_right = math::vector3{camera_rotation[0]};
  const auto camera_up = math::vector3{camera_rotation[1]};

  _uniform_handler.push("camera_right", camera_right);
  _uniform_handler.push("camera_up", camera_up);

  _cached_csm = _build_csm();

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
  const auto camera_projection = camera.projection(0.1f, 100.0f);

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
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  const auto& viewport = graphics_module.viewport();

  const auto& camera = _graph.get_component<scenes::camera>(_camera);

  const auto world = _graph.world_transform(_camera);
  const auto view = math::matrix4x4::inverted(world);
  const auto projection = camera.projection();

  const auto inverse_view_projection = math::matrix4x4::inverted(projection * view);

  const auto px = position.x() + 0.5f;
  const auto py = position.y() + 0.5f;

  const auto x = (2.0f * px) / viewport.x() - 1.0f;
  const auto y = (2.0f * py) / viewport.y() - 1.0f;

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

auto scene_environment::_build_light_space_for_slice(const scenes::camera& camera, const math::matrix4x4& camera_world, const math::vector3& light_direction, const std::float_t slice_near, const std::float_t slice_far, const std::uint32_t shadow_resolution) -> math::matrix4x4 {
  const auto camera_view = math::matrix4x4::inverted(camera_world);
  const auto camera_projection = camera.projection(slice_near, slice_far);

  const auto inv_view_projection = math::matrix4x4::inverted(camera_projection * camera_view);

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

  auto corners_world = std::array<math::vector3, 8u>{};
  auto center_world = math::vector3::zero;

  for (auto i = 0u; i < 8u; ++i) {
    auto corner_world = inv_view_projection * frustum_corners_clip[i];
    corner_world /= corner_world.w();

    corners_world[i] = math::vector3{corner_world};
    center_world += corners_world[i];
  }

  center_world /= 8.0f;

  const auto light_dir = math::vector3::normalized(light_direction);

  auto up = math::vector3::up;

  if (std::abs(math::vector3::dot(light_dir, up)) > 0.99f) {
    up = math::vector3{1.0f, 0.0f, 0.0f};
  }

  const auto light_view_initial = math::matrix4x4::look_at(center_world, center_world + light_dir, up);

  auto min_z = std::numeric_limits<std::float_t>::max();
  auto max_z = std::numeric_limits<std::float_t>::lowest();

  for (const auto& corner : corners_world) {
    const auto p = light_view_initial * math::vector4{corner, 1.0f};

    min_z = std::min(min_z, p.z());
    max_z = std::max(max_z, p.z());
  }

  static constexpr auto z_caster_padding = 50.0f;
  const auto pull_back = std::abs(min_z) + z_caster_padding;

  const auto light_position = center_world - light_dir * pull_back;
  const auto light_view = math::matrix4x4::look_at(light_position, center_world, up);

  auto min_bounds = math::vector3{std::numeric_limits<std::float_t>::max()};
  auto max_bounds = math::vector3{std::numeric_limits<std::float_t>::lowest()};

  for (const auto& corner : corners_world) {
    const auto p4 = light_view * math::vector4{corner, 1.0f};
    const auto p = math::vector3{p4};

    min_bounds = math::vector3::min(min_bounds, p);
    max_bounds = math::vector3::max(max_bounds, p);
  }

  static constexpr auto xy_padding = 10.0f;
  const auto extent_x = max_bounds.x() - min_bounds.x();
  const auto extent_y = max_bounds.y() - min_bounds.y();
  const auto extent = std::max(extent_x, extent_y) + 2.0f * xy_padding;

  const auto texel_size = extent / static_cast<std::float_t>(shadow_resolution);

  const auto center_ls = (min_bounds + max_bounds) * 0.5f;

  auto min_x = center_ls.x() - extent * 0.5f;
  auto min_y = center_ls.y() - extent * 0.5f;

  min_x = std::floor(min_x / texel_size) * texel_size;
  min_y = std::floor(min_y / texel_size) * texel_size;

  const auto max_x = min_x + texel_size * static_cast<std::float_t>(shadow_resolution);
  const auto max_y = min_y + texel_size * static_cast<std::float_t>(shadow_resolution);

  const auto z_range = max_bounds.z() - min_bounds.z();
  const auto z_padding = std::max(z_range * 2.0f, 50.0f);

  auto near_plane = -(max_bounds.z() + z_padding);
  auto far_plane  = -(min_bounds.z() - z_padding);

  if (near_plane > far_plane) {
    std::swap(near_plane, far_plane);
  }

  const auto light_projection = math::matrix4x4::orthographic(
    min_x, max_x,
    min_y, max_y,
    near_plane, far_plane
  );

  return light_projection * light_view;
}

auto scene_environment::_build_csm() -> csm_data {
  const auto& camera = _graph.get_component<scenes::camera>(_camera);
  const auto camera_world = _graph.world_transform(_camera);

  const auto camera_near = camera.near_plane();
  const auto camera_far = camera.far_plane();

  static constexpr auto lambda = 0.65f;
  static constexpr auto shadow_resolution = std::uint32_t{2048u};

  const auto splits = _compute_csm_splits(camera_near, camera_far, lambda);

  auto result = csm_data{};

  auto slice_near = camera_near;

  for (auto i = 0u; i < csm_cascade_count; ++i) {
    const auto slice_far = splits[i];

    result.light_spaces[i] = _build_light_space_for_slice(camera, camera_world, _light.direction(), slice_near, slice_far, shadow_resolution);

    slice_near = slice_far;
  }

  result.cascade_splits = math::vector4{splits[0], splits[1], splits[2], camera_far};

  return result;
}

} // namespace sbx::scenes
