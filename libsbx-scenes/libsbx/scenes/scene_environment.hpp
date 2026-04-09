// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_SCENE_ENVIRONMENT_HPP_
#define LIBSBX_SCENES_SCENE_ENVIRONMENT_HPP_

#include <array>
#include <cmath>
#include <limits>
#include <algorithm>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/ray.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene_graph.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/directional_light.hpp>

namespace sbx::scenes {

class scene_environment {

  static constexpr auto csm_cascade_count = std::uint32_t{4u};

  struct csm_data {
    std::array<math::matrix4x4, csm_cascade_count> light_spaces{};
    math::vector4 cascade_splits{};
  }; // struct csm_data

public:

  scene_environment(scene_graph& graph, const scenes::node camera, const math::vector3& light_direction, const math::color& light_color);

  auto light() -> directional_light& {
    return _light;
  }

  auto light() const -> const directional_light& {
    return _light;
  }

  auto camera() const -> scenes::node {
    return _camera;
  }

  auto set_active_camera(const scenes::node camera) -> void {
    _camera = camera;
  }

  auto cascade_light_spaces() const -> const std::array<math::matrix4x4, csm_cascade_count>& {
    return _cached_csm.light_spaces;
  }

  static constexpr auto cascade_count() -> std::uint32_t {
    return csm_cascade_count;
  }

  auto uniform_handler() -> graphics::uniform_handler& {
    return _uniform_handler;
  }

  auto update_uniforms() -> void;

  auto view_projection() const -> const math::matrix4x4& {
    return _view_projection;
  }

  auto light_space() -> math::matrix4x4;

  auto screen_point_to_ray(const math::vector2& position) -> math::ray;

  auto set_render_target_size(const math::vector2u& size) -> void {
    _render_target_size = size;
  }

  auto render_target_size() const -> const math::vector2u& {
    return _render_target_size;
  }

private:

  static auto _lerp_float(const std::float_t a, const std::float_t b, const std::float_t t) -> std::float_t {
    return a + (b - a) * t;
  }

  static auto _compute_csm_splits(const std::float_t near_plane, const std::float_t far_plane, const std::float_t lambda) -> std::array<std::float_t, csm_cascade_count>;

  static auto _build_light_space_for_slice(const scenes::camera& camera, std::float_t aspect_ratio, const math::matrix4x4& camera_world, const math::vector3& light_direction, const std::float_t slice_near, const std::float_t slice_far, const std::uint32_t shadow_resolution) -> math::matrix4x4;

  auto _build_csm(std::float_t aspect_ratio) -> csm_data;

  scene_graph& _graph;

  
  scenes::node _camera;
  directional_light _light;

  math::vector2u _render_target_size{0, 0};
  math::matrix4x4 _view_projection{math::matrix4x4::identity};

  graphics::uniform_handler _uniform_handler;
  csm_data _cached_csm;

}; // class scene_environment

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SCENE_ENVIRONMENT_HPP_
