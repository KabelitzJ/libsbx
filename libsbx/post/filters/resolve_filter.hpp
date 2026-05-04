// SPDX-License-Identifier: MIT
#ifndef LIBSBX_POST_FILTERS_RESOLVE_FILTER_HPP_
#define LIBSBX_POST_FILTERS_RESOLVE_FILTER_HPP_

#include <string>
#include <unordered_map>

#include <libsbx/scenes/components/transform.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/buffers/uniform_buffer.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/directional_light.hpp>
#include <libsbx/scenes/components/point_light.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/skybox.hpp>

#include <libsbx/post/filter.hpp>

namespace sbx::post {

template<bool Transparent>
class resolve_filter final : public filter {

  using base = filter;

  inline static constexpr auto max_point_lights = std::size_t{32};
  inline static constexpr auto max_directional_lights = std::size_t{4};

  struct point_light_data {
    alignas(16) math::vector4 position;
    alignas(16) math::color color;
  }; // struct point_light_data

  struct directional_light_data {
    alignas(16) math::vector4 direction;
    alignas(16) math::color color;
  }; // struct directional_light_data

  inline static const auto pipeline_definition = graphics::pipeline_definition{
    .depth = graphics::depth::disabled,
    .uses_transparency = Transparent,
    .rasterization_state = graphics::rasterization_state{
      .polygon_mode = graphics::polygon_mode::fill,
      .cull_mode = graphics::cull_mode::none,
      .front_face = graphics::front_face::counter_clockwise
    }
  };

  static constexpr auto _default_shader_path() -> std::string_view {
    if constexpr (Transparent) {
      return "engine://shaders/resolve_transparent";
    } else {
      return "engine://shaders/resolve_opaque";
    }
  }

public:

  resolve_filter(const std::vector<graphics::attachment_description>& attachments, std::vector<std::pair<std::string, std::string>>&& attachment_names, const std::filesystem::path& path = _default_shader_path())
  : base{attachments, path, pipeline_definition},
    _push_handler{base::pipeline()},
    _point_lights_storage_handler{},
    _directional_lights_storage_handler{},
    _attachment_names{std::move(attachment_names)} { }

  ~resolve_filter() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    SBX_PROFILE_SCOPE("resolve_filter::render");

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

    auto& scene = scenes_module.active_scene();
    auto& environment = scene.environment();
    auto& graph = scene.graph();

    auto& pipeline = base::pipeline();
    auto& descriptor_handler = base::descriptor_handler();

    pipeline.bind(command_buffer);

    auto point_light_nodes = graph.query<scenes::point_light>();

    auto point_lights = std::vector<point_light_data>{};
    point_lights.reserve(max_point_lights);
    auto point_light_count = std::uint32_t{0};

    for (const auto& node : point_light_nodes) {
      const auto model = graph.world_transform(node);

      const auto& light = graph.get_component<scenes::point_light>(node);

      const auto position = math::vector3{model[3]};

      point_lights.push_back(point_light_data{math::vector4{position, light.radius()}, light.color()});

      ++point_light_count;

      if (point_light_count >= max_point_lights) {
        break;
      }
    }

    auto directional_light_nodes = graph.query<scenes::directional_light>();

    auto directional_lights = std::vector<directional_light_data>{};
    directional_lights.reserve(max_directional_lights);
    auto directional_light_count = std::uint32_t{0};

    for (const auto& node : directional_light_nodes) {
      const auto& light = graph.get_component<scenes::directional_light>(node);

      const auto direction = math::vector3::normalized(light.direction());

      directional_lights.push_back(directional_light_data{math::vector4{direction, 0.0f}, light.color()});

      ++directional_light_count;

      if (directional_light_count >= max_directional_lights) {
        break;
      }
    }

    if constexpr (!Transparent) {
      _point_lights_storage_handler.push(std::span<const point_light_data>{point_lights.data(), point_light_count});
      _directional_lights_storage_handler.push(std::span<const directional_light_data>{directional_lights.data(), directional_light_count});

      _push_handler.push("point_light_count", point_light_count);
      _push_handler.push("directional_light_count", directional_light_count);

      descriptor_handler.push("scene", environment.uniform_handler());
      descriptor_handler.push("buffer_point_lights", _point_lights_storage_handler);
      descriptor_handler.push("buffer_directional_lights", _directional_lights_storage_handler);
    }

    for (const auto& [name, attachment] : _attachment_names) {
      descriptor_handler.push(name, graphics_module.attachment(attachment));
    }

    if constexpr (!Transparent) {
      auto camera_node = environment.camera();

      const auto& skybox = graph.get_component<scenes::skybox>(camera_node);

      descriptor_handler.push("brdf_image", graphics_module.get_resource<graphics::image2d>(skybox.brdf_image));
      descriptor_handler.push("irradiance_image", graphics_module.get_resource<graphics::cube_image>(skybox.irradiance_image));
      descriptor_handler.push("prefiltered_image", graphics_module.get_resource<graphics::cube_image>(skybox.prefiltered_image));
    }

    if (!descriptor_handler.update(pipeline)) {
      return;
    }

    descriptor_handler.bind_descriptors(command_buffer);

    if constexpr (!Transparent) {
      _push_handler.bind(command_buffer);
    }

    command_buffer.draw(3, 1, 0, 0);
  }

private:


  graphics::push_handler _push_handler;
  graphics::storage_handler _point_lights_storage_handler;
  graphics::storage_handler _directional_lights_storage_handler;
  std::vector<std::pair<std::string, std::string>> _attachment_names;

}; // class resolve_filter

using resolve_opaque_filter = resolve_filter<false>;

using resolve_transparent_filter = resolve_filter<true>;

} // namespace sbx::post

#endif // LIBSBX_POST_FILTERS_RESOLVE_FILTER_HPP_
