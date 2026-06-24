// SPDX-License-Identifier: MIT
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/reflection/reflection.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_graph.hpp>

#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/scenes/components/skybox.hpp>
#include <libsbx/scenes/components/directional_light.hpp>
#include <libsbx/scenes/components/point_light.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>

namespace sbx::scenes {

scenes_module::scenes_module() {
  // _asset_io_registry.register_loader("images", [](scenes::asset_registry& registry, const utility::hashed_string& name, const YAML::Node& node) -> void {
  //   registry.request_image(name, node["path"].as<std::string>());
  // });

  // _asset_io_registry.register_loader("cube_images", [](scenes::asset_registry& registry, const utility::hashed_string& name, const YAML::Node& node) -> void {
  //   const auto path = node["path"].as<std::string>();
  //   const auto suffix = node["extention"] ? fmt::format(".{}", node["extention"].as<std::string>()) : std::string{".png"};
  //   const auto format = reflection::from_string<graphics::format>(node["format"].as<std::string>("")).value_or(graphics::format::r8g8b8a8_srgb);

  //   registry.request_cube_image(name, path, suffix, format);
  // });

  _component_serializer.register_component<scenes::skybox>(
    "skybox",
    [](YAML::Node& out, const scenes::skybox& skybox, component_serializer::asset_set& assets) -> void {
      out["environment"] = skybox.environment;
      out["tint"] = skybox.tint;

      assets.insert(skybox.environment);
    },
    [](const YAML::Node& node, scene_graph& graph, scenes::node n) -> void {
      auto skybox = scenes::skybox{};

      skybox.environment = node["environment"].as<math::uuid>();

      if (const auto tint = node["tint"]; tint) {
        skybox.tint = tint.as<math::color>();
      }

      graph.add_component<scenes::skybox>(n, skybox);
    }
  );

  _component_serializer.register_component<scenes::point_light>(
    "point_light",
    [](YAML::Node& out, const scenes::point_light& point_light, [[maybe_unused]] component_serializer::asset_set& assets) -> void {
      out["color"] = point_light.color();
      out["radius"] = point_light.radius();
    },
    [](const YAML::Node& node, scene_graph& graph, scenes::node n) -> void {
      graph.add_component<scenes::point_light>(n, node["color"].as<math::color>(), node["radius"].as<std::float_t>());
    }
  );

  _component_serializer.register_component<scenes::static_mesh>(
    "static_mesh",
    [](YAML::Node& out, const scenes::static_mesh& static_mesh, component_serializer::asset_set& assets) -> void {
      out["mesh"] = static_mesh.mesh_id();

      assets.insert(static_mesh.mesh_id());

      auto submeshes = YAML::Node{};

      for (const auto& submesh : static_mesh.submeshes()) {
        auto submesh_node = YAML::Node{};

        submesh_node["index"] = submesh.index;
        submesh_node["material"] = submesh.material;

        submeshes.push_back(submesh_node);

        assets.insert(submesh.material);
      }

      out["submeshes"] = submeshes;
    },
    [](const YAML::Node& node, scene_graph& graph, scenes::node n) -> void {
      auto submeshes = std::vector<scenes::static_mesh::submesh>{};

      if (const auto submeshes_node = node["submeshes"]; submeshes_node && submeshes_node.IsSequence()) {
        for (const auto& submesh_node : submeshes_node) {
          submeshes.push_back(scenes::static_mesh::submesh{submesh_node["index"].as<std::uint32_t>(), submesh_node["material"].as<math::uuid>()});
        }
      }

      graph.add_component<scenes::static_mesh>(n, node["mesh"].as<math::uuid>(), std::move(submeshes));
    }
  );

  _component_serializer.register_component<scenes::camera>(
    "camera",
    [](YAML::Node& out, const scenes::camera& camera, [[maybe_unused]] component_serializer::asset_set& assets) -> void {
      out["field_of_view"] = camera.field_of_view().to_degrees().value();
      out["near"] = camera.near_plane();
      out["far"] = camera.far_plane();
    },
    [](const YAML::Node& node, scene_graph& graph, scenes::node n) -> void {
      graph.add_component<scenes::camera>(n, math::angle{math::degree{node["field_of_view"].as<std::float_t>(60.0f)}}, node["near"].as<std::float_t>(0.1f), node["far"].as<std::float_t>(1000.0f));
    }
  );
}

scenes_module::~scenes_module() {

}

auto scenes_module::update() -> void {
  SBX_PROFILE_SCOPE("scenes_module::update");

  if (!_active_scene) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& viewports = graphics_module.viewports();
  auto& environment = _active_scene->environment();

  environment.set_render_target_size(viewports.size(_scene_viewport));
  environment.update_uniforms();
}

auto scenes_module::create_scene(const std::string& name) -> scenes::scene& {
  auto key = utility::hashed_string{name};

  auto [entry, inserted] = _scenes.emplace(key, std::make_unique<scenes::scene>(name));

  _active_scene = entry->second.get();

  return *_active_scene;
}

auto scenes_module::load_scene(const utility::hashed_string& name, const std::filesystem::path& path) -> scenes::scene& {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto [entry, inserted] = _scenes.emplace(name, std::make_unique<scenes::scene>(assets_module.resolve_path(path)));

  _active_scene = entry->second.get();

  return *_active_scene;
}

auto scenes_module::close_scene(const utility::hashed_string& name) -> void {
  auto entry = _scenes.find(name);

  if (entry == _scenes.end()) {
    return;
  }

  if (_active_scene == entry->second) {
    _active_scene = nullptr;
  }

  _scenes.erase(entry);

  if (_active_scene == nullptr && !_scenes.empty()) {
    _active_scene = _scenes.begin()->second.get();
  }
}

auto scenes_module::set_active_scene(const utility::hashed_string& name) -> void {
  if (auto entry = _scenes.find(name); entry != _scenes.end()) {
    _active_scene = entry->second.get();
  }
}

auto scenes_module::debug_lines() const -> const std::vector<line>& {
  return _debug_lines;
}

auto scenes_module::clear_debug_lines() -> void {
  _debug_lines.clear();
}

auto scenes_module::add_debug_line(const sbx::math::vector3& start, const sbx::math::vector3& end, const sbx::math::color& color) -> void {
  _debug_lines.push_back(line{
    .position = sbx::math::vector4{start, 1.0f},
    .color = color
  });

  _debug_lines.push_back(line{
    .position = sbx::math::vector4{end, 1.0f},
    .color = color
  });
}

auto scenes_module::add_coordinate_arrows(const math::matrix4x4& transform, std::float_t length) -> void {
  const auto origin = math::vector3{transform[3]};

  const auto x_axis = math::vector3::normalized(transform[0]);
  const auto y_axis = math::vector3::normalized(transform[1]);
  const auto z_axis = math::vector3::normalized(transform[2]);

  add_debug_line(origin, origin + x_axis * length, math::color::red());
  add_debug_line(origin, origin + y_axis * length, math::color::green());
  add_debug_line(origin, origin + z_axis * length, math::color::blue());
}

auto scenes_module::add_debug_plane(const sbx::math::vector3& origin, const sbx::math::vector3& v1, const sbx::math::vector3& v2, std::uint32_t n1, std::uint32_t n2, std::float_t s1, std::float_t s2, const sbx::math::color& color, const sbx::math::color& outline) -> void {
  add_debug_line(origin - s1 / 2.0f * v1 - s2 / 2.0f * v2, origin - s1 / 2.0f * v1 + s2 / 2.0f * v2, outline);
  add_debug_line(origin + s1 / 2.0f * v1 - s2 / 2.0f * v2, origin + s1 / 2.0f * v1 + s2 / 2.0f * v2, outline);
  add_debug_line(origin - s1 / 2.0f * v1 + s2 / 2.0f * v2, origin + s1 / 2.0f * v1 + s2 / 2.0f * v2, outline);
  add_debug_line(origin - s1 / 2.0f * v1 - s2 / 2.0f * v2, origin + s1 / 2.0f * v1 - s2 / 2.0f * v2, outline);

  for (auto i = 1u; i < n1; i++) {
    const auto t = (static_cast<std::float_t>(i) - static_cast<std::float_t>(n1) / 2.0f) * s1 / static_cast<std::float_t>(n1);
    const auto o1 = origin + t * v1;
    add_debug_line(o1 - s2 / 2.0f * v2, o1 + s2 / 2.0f * v2, color);
  }

  for (auto i = 1u; i < n2; i++) {
    const auto t = (static_cast<std::float_t>(i) - static_cast<std::float_t>(n2) / 2.0f) * s2 / static_cast<std::float_t>(n2);
    const auto o2 = origin + t * v2;
    add_debug_line(o2 - s1 / 2.0f * v1, o2 + s1 / 2.0f * v1, color);
  }
}

auto scenes_module::add_debug_volume(const math::matrix4x4& matrix, const math::volume& volume, const sbx::math::color& color) -> void {
  const auto transformed = math::volume::transformed(volume, matrix);
  const auto corners = transformed.corners();

  add_debug_line(corners[0], corners[1], color); add_debug_line(corners[2], corners[3], color);
  add_debug_line(corners[4], corners[5], color); add_debug_line(corners[6], corners[7], color);
  add_debug_line(corners[0], corners[2], color); add_debug_line(corners[1], corners[3], color);
  add_debug_line(corners[4], corners[6], color); add_debug_line(corners[5], corners[7], color);
  add_debug_line(corners[0], corners[4], color); add_debug_line(corners[1], corners[5], color);
  add_debug_line(corners[2], corners[6], color); add_debug_line(corners[3], corners[7], color);
}

auto scenes_module::add_debug_box(const math::matrix4x4& matrix, const math::volume& volume, const sbx::math::color& color) -> void {
  auto corners = std::vector<math::vector3>{};
  corners.reserve(8u);

  for (const auto& corner : volume.corners()) {
    corners.push_back(math::vector3{matrix * math::vector4{corner, 1.0f}});
  }

  add_debug_line(corners[0], corners[1], color); add_debug_line(corners[2], corners[3], color);
  add_debug_line(corners[4], corners[5], color); add_debug_line(corners[6], corners[7], color);
  add_debug_line(corners[0], corners[2], color); add_debug_line(corners[1], corners[3], color);
  add_debug_line(corners[4], corners[6], color); add_debug_line(corners[5], corners[7], color);
  add_debug_line(corners[0], corners[4], color); add_debug_line(corners[1], corners[5], color);
  add_debug_line(corners[2], corners[6], color); add_debug_line(corners[3], corners[7], color);
}

auto scenes_module::add_debug_circle(const math::vector3& center, const std::float_t radius, const math::vector3& normal, const math::color& color, const std::uint32_t segments) -> void {
  const auto up = std::abs(math::vector3::dot(normal, math::vector3::up)) < 0.99f ? math::vector3::up : math::vector3::right;
  const auto tangent = math::vector3::normalized(math::vector3::cross(normal, up));
  const auto bitangent = math::vector3::normalized(math::vector3::cross(normal, tangent));

  for (auto i = 0u; i < segments; ++i) {
    auto theta0 = (2.0f * math::two_pi) * (static_cast<float>(i) / segments);
    auto theta1 = (2.0f * math::two_pi) * (static_cast<float>(i + 1) / segments);

    const auto point0 = center + (tangent * std::cos(theta0) + bitangent * std::sin(theta0)) * radius;
    const auto point1 = center + (tangent * std::cos(theta1) + bitangent * std::sin(theta1)) * radius;

    add_debug_line(point0, point1, color);
  }
}

auto scenes_module::add_debug_sphere(const math::vector3& center, const std::float_t radius, const math::color& color, const std::uint32_t segments) -> void {
  add_debug_circle(center, radius, math::vector3::backward, color, segments);
  add_debug_circle(center, radius, math::vector3::right, color, segments);
  add_debug_circle(center, radius, math::vector3::up, color, segments);
}

auto scenes_module::add_debug_frustum(const math::matrix4x4& view, const math::matrix4x4& projection, const sbx::math::color& color) -> void {
  const auto corners = std::array<sbx::math::vector3, 8u>{
    math::vector3(-1, -1, 0), math::vector3(+1, -1, 0),
    math::vector3(+1, +1, 0), math::vector3(-1, +1, 0),
    math::vector3(-1, -1, 1), math::vector3(+1, -1, 1),
    math::vector3(+1, +1, 1), math::vector3(-1, +1, 1)
  };

  auto points = std::array<sbx::math::vector3, 8u>{};

  for (auto i = 0u; i < 8u; ++i) {
    auto q = math::matrix4x4::inverted(view) * math::matrix4x4::inverted(projection) * math::vector4{corners[i], 1.0f};
    points[i] = math::vector3{q.x() / q.w(), q.y() / q.w(), q.z() / q.w()};
  }

  add_debug_line(points[0], points[4], color); add_debug_line(points[1], points[5], color);
  add_debug_line(points[2], points[6], color); add_debug_line(points[3], points[7], color);

  add_debug_line(points[0], points[1], color); add_debug_line(points[1], points[2], color);
  add_debug_line(points[2], points[3], color); add_debug_line(points[3], points[0], color);
  add_debug_line(points[0], points[2], color); add_debug_line(points[1], points[3], color);

  add_debug_line(points[4], points[5], color); add_debug_line(points[5], points[6], color);
  add_debug_line(points[6], points[7], color); add_debug_line(points[7], points[4], color);
  add_debug_line(points[4], points[6], color); add_debug_line(points[5], points[7], color);

  const auto grid_color = color * 0.7f;
  const auto grid_lines = 100;

  auto p1 = points[0]; auto p2 = points[1];
  auto s1 = (points[4] - points[0]) / static_cast<std::float_t>(grid_lines);
  auto s2 = (points[5] - points[1]) / static_cast<std::float_t>(grid_lines);
  for (auto i = 0; i != grid_lines; i++, p1 += s1, p2 += s2) { add_debug_line(p1, p2, grid_color); }

  p1 = points[2]; p2 = points[3];
  s1 = (points[6] - points[2]) / static_cast<std::float_t>(grid_lines); s2 = (points[7] - points[3]) / static_cast<std::float_t>(grid_lines);
  for (auto i = 0; i != grid_lines; i++, p1 += s1, p2 += s2) { add_debug_line(p1, p2, grid_color); }

  p1 = points[0]; p2 = points[3];
  s1 = (points[4] - points[0]) / static_cast<std::float_t>(grid_lines); s2 = (points[7] - points[3]) / static_cast<std::float_t>(grid_lines);
  for (auto i = 0; i != grid_lines; i++, p1 += s1, p2 += s2) { add_debug_line(p1, p2, grid_color); }

  p1 = points[1]; p2 = points[2];
  s1 = (points[5] - points[1]) / static_cast<std::float_t>(grid_lines); s2 = (points[6] - points[2]) / static_cast<std::float_t>(grid_lines);
  for (auto i = 0; i != grid_lines; i++, p1 += s1, p2 += s2) { add_debug_line(p1, p2, grid_color); }
}

} // namespace sbx::scenes
