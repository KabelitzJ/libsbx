#ifndef DEMO_TERRAIN_TERRAIN_MODULE_HPP_
#define DEMO_TERRAIN_TERRAIN_MODULE_HPP_

#include <numbers>

#include <libsbx/units/time.hpp>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/random.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>

#include <libsbx/physics/physics.hpp>

#include <demo/terrain/planet.hpp>

namespace demo {

class terrain_module final : public sbx::core::module<terrain_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<sbx::graphics::graphics_module, sbx::scenes::scenes_module>{});

public:

  terrain_module() {
    
  }

  ~terrain_module() override = default;

  auto load_terrain_in_scene() -> void {
    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
    
    auto& scene = scenes_module.scene();
    
    const auto chunk_size = sbx::math::vector2u{25u, 25u};

    // scene.add_mesh<sbx::models::mesh>("terrain", _generate_plane(chunk_size, sbx::math::vector2u{1u, 1u}));

    scene.add_image("prototype", "demo/assets/textures/prototype_white.png");

    const auto grid = sbx::math::vector2{10.0f, 10.0f};

    const auto offset = sbx::math::vector2{chunk_size.x() * grid.x() * 0.5f, chunk_size.y() * grid.y() * 0.5f};

    _node = scene.create_node("Terrain");

    auto& transform = scene.get_component<sbx::scenes::transform>(_node);
    transform.set_position(sbx::math::vector3{0, 0.5f, 0});

    auto& terrain_material = scene.add_material<sbx::models::material>("terrain");
    terrain_material.albedo.image = scene.get_image("prototype");

    for (auto y = 0u; y < grid.y(); ++y) {
      for (auto x = 0u; x < grid.x(); ++x) {
        auto chunk = scene.create_child_node(_node, fmt::format("Chunk{}{}", x, y));

        scene.add_component<sbx::scenes::static_mesh>(chunk, scene.get_mesh("cube"), scene.get_material("terrain"));

        auto& transform = scene.get_component<sbx::scenes::transform>(chunk);
        transform.set_position(sbx::math::vector3{x * chunk_size.x() - offset.x(), 0.0f, y * chunk_size.y() - offset.y()});
        transform.set_scale(sbx::math::vector3{25, 0.5, 25});

        scene.add_component<sbx::physics::rigidbody>(chunk, sbx::units::kilogram{0});
        scene.add_component<sbx::physics::collider>(chunk, sbx::physics::box{sbx::math::vector3{12.5, 0.25, 12.5}});
      }
    }

    // auto icosphere_tile_mesh = demo::icosphere_tile_mesh{4u, 0.02f};
    // _planet_id = assets_module.add_asset<sbx::models::mesh>(std::make_unique<sbx::models::mesh>(icosphere_tile_mesh.get_vertices(), icosphere_tile_mesh.get_indices(), icosphere_tile_mesh.get_bounds()));

    // _planet_id = assets_module.add_asset<sbx::models::mesh>(_generate_icosphere(50.0f, 5u, 0.1f));

    // auto planet = scene.create_node("Planet");
    // auto planet_submeshes = std::vector<sbx::scenes::static_mesh::submesh>{};
    // planet_submeshes.emplace_back(sbx::scenes::static_mesh::submesh{
    //   .index = 0u,
    //   .tint = sbx::math::color::red(),
    //   .material = sbx::scenes::static_mesh::material{
    //     .metallic = 0.0f,
    //     .roughness = 1.0f,
    //     .flexibility = 0.0f,
    //     .anchor_height = 0.0f
    //   }
    // });

    // scene.add_component<sbx::scenes::static_mesh>(planet, _planet_id, planet_submeshes);

    // auto& planet_transform = scene.get_component<sbx::scenes::transform>(planet);
    // planet_transform.set_position(sbx::math::vector3{0.0f, 0.0f, 0.0f});
  }

  auto update() -> void override {

  }

private:

  // auto _generate_polygon(const std::vector<sbx::math::vector2>& points) -> std::unique_ptr<sbx::models::mesh> {
  //   auto vertices = std::vector<sbx::models::vertex3d>{};
  //   auto indices = std::vector<std::uint32_t>{};

  //   for (const auto& point : points) {
  //     vertices.emplace_back(sbx::math::vector3{point.x(), 0.0f, point.y()}, sbx::math::vector3::up, sbx::math::vector2{0.0f, 0.0f});
  //   }

  //   for (auto i = 0u; i < points.size() - 2u; ++i) {
  //     indices.emplace_back(0u);
  //     indices.emplace_back(i + 1u);
  //     indices.emplace_back(i + 2u);
  //   }

  //   return std::make_unique<sbx::models::mesh>(std::move(vertices), std::move(indices));
  // }

  auto _generate_plane(const sbx::math::vector2u& size, const sbx::math::vector2u& subdivisions) -> std::unique_ptr<sbx::models::mesh> {
    auto vertices = std::vector<sbx::models::vertex3d>{};
    auto indices = std::vector<std::uint32_t>{};

    const auto x_vertices = subdivisions.x() + 1u;
    const auto y_vertices = subdivisions.y() + 1u;

    const auto tile_size = sbx::math::vector2{static_cast<std::float_t>(size.x() / subdivisions.x()), static_cast<std::float_t>(size.y() / subdivisions.y())};
    
    const auto offset = sbx::math::vector2{static_cast<std::float_t>(size.x() / 2.0f), static_cast<std::float_t>(size.y() / 2.0f)};
    
    // Generate vertices
    for (auto y = 0u; y < subdivisions.y() + 1u; ++y) {
      for (auto x = 0u; x < subdivisions.x() + 1u; ++x) {
        const auto position = sbx::math::vector3{static_cast<std::float_t>(x * tile_size.x() - offset.x()), 0.0f, static_cast<std::float_t>(y * tile_size.y() - offset.y())};
        const auto normal = sbx::math::vector3::up;
        const auto tangent = sbx::math::vector4(sbx::math::vector3::right, 1.0f);
        const auto uv = sbx::math::vector2{static_cast<std::float_t>(x % 2), static_cast<std::float_t>(y % 2)};

        vertices.emplace_back(position, normal, uv, tangent);
      }
    }

    // Calculate indices

    const auto vertex_count = subdivisions.x() + 1u;

    for (auto i = 0u; i < vertex_count * vertex_count - vertex_count; ++i) {
      if ((i + 1u) % vertex_count == 0u) {
        continue;
      }

      indices.emplace_back(i);
      indices.emplace_back(i + vertex_count);
      indices.emplace_back(i + vertex_count + 1u);

      indices.emplace_back(i);
      indices.emplace_back(i + vertex_count + 1u);
      indices.emplace_back(i + 1u);
    }

    const auto half_width = static_cast<std::float_t>(size.x()) / 2.0f;
    const auto half_height = static_cast<std::float_t>(size.y()) / 2.0f;

    const auto min = sbx::math::vector3{-half_width, -0.1f, -half_height};
    const auto max = sbx::math::vector3{half_width, 0.1f, half_height};

    return std::make_unique<sbx::models::mesh>(std::move(vertices), std::move(indices), sbx::math::volume{min, max});
  }

  static auto spherical_uv(const sbx::math::vector3& p) -> sbx::math::vector2 {
    const auto pi = std::numbers::pi_v<std::float_t>;
    const float u = 0.5f + std::atan2(p.z(), p.x()) / (2.0f * pi);
    const float v = 0.5f - std::asin(p.y()) / pi;
    return sbx::math::vector2{u, v};
  }

  auto _generate_icosphere(std::float_t radius, std::uint32_t subdivisions, std::float_t distortion_strength = 0.0f) -> std::unique_ptr<sbx::models::mesh> {
    using triangle = std::array<std::uint32_t, 3u>;

    struct corner {
      std::uint32_t face_index;
      std::float_t angle;
    }; // struct corner

    // const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    static constexpr auto phi = std::numbers::phi_v<std::float_t>;

    auto positions = std::vector<sbx::math::vector3>{
      {-1,  phi,  0}, { 1,  phi,  0}, {-1, -phi,  0}, { 1, -phi,  0},
      { 0, -1,  phi}, { 0,  1,  phi}, { 0, -1, -phi}, { 0,  1, -phi},
      { phi,  0, -1}, { phi,  0,  1}, {-phi,  0, -1}, {-phi,  0,  1}
    };

    for (auto& position : positions) { 
      position = sbx::math::vector3::normalized(position) * radius;
    }

    auto faces = std::vector<triangle>{
      {0,11,5}, {0,5,1}, {0,1,7}, {0,7,10}, {0,10,11},
      {1,5,9}, {5,11,4}, {11,10,2}, {10,7,6}, {7,1,8},
      {3,9,4}, {3,4,2}, {3,2,6}, {3,6,8}, {3,8,9},
      {4,9,5}, {2,4,11}, {6,2,10}, {8,6,7}, {9,8,1}
    };

    auto midpoint_cache = std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t>{};

    auto get_midpoint = [&](std::uint32_t i1, std::uint32_t i2) -> std::uint32_t {
      auto key = std::minmax(i1, i2);

      if (auto it = midpoint_cache.find(key); it != midpoint_cache.end()) {
        return it->second;
      }

      auto midpoint = (positions[i1] + positions[i2]) * 0.5f;

      if (distortion_strength > 0.0f) {
        midpoint += sbx::math::vector3{
          sbx::math::random::next<std::float_t>(-1.0f, 1.0f) * distortion_strength,
          sbx::math::random::next<std::float_t>(-1.0f, 1.0f) * distortion_strength,
          sbx::math::random::next<std::float_t>(-1.0f, 1.0f) * distortion_strength
        };
      }

      midpoint = sbx::math::vector3::normalized(midpoint) * radius;

      positions.push_back(midpoint);

      return (midpoint_cache[key] = static_cast<std::uint32_t>(positions.size() - 1));
    };

    for (auto i = 0u; i < subdivisions; ++i) {
      auto new_faces = std::vector<triangle>{};

      for (const auto& [a, b, c] : faces) {
        const auto ab = get_midpoint(a, b);
        const auto bc = get_midpoint(b, c);
        const auto ca = get_midpoint(c, a);

        new_faces.push_back({a, ab, ca});
        new_faces.push_back({b, bc, ab});
        new_faces.push_back({c, ca, bc});
        new_faces.push_back({ab, bc, ca});
      }
  
      faces = std::move(new_faces);
    }

    // --- Dual generation ---
    auto face_centroids = std::vector<sbx::math::vector3>{};
    face_centroids.reserve(faces.size());

    for (const auto& [a, b, c] : faces) {
      face_centroids.push_back(sbx::math::vector3::normalized((positions[a] + positions[b] + positions[c]) / 3.0f) * radius);
    }

    auto vertex_to_faces = std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>{};

    for (auto i = 0u; i < faces.size(); ++i) {
      const auto& [a, b, c] = faces[i];

      vertex_to_faces[a].push_back(i);
      vertex_to_faces[b].push_back(i);
      vertex_to_faces[c].push_back(i);
    }

    auto vertices = std::vector<sbx::models::vertex3d>{};
    auto indices = std::vector<std::uint32_t>{};

    for (const auto& [vertex_idx, face_indices] : vertex_to_faces) {
      const auto center_position = sbx::math::vector3::normalized(positions[vertex_idx]) * radius;
      const auto normal = sbx::math::vector3::normalized(center_position);
      const auto tangent = sbx::math::vector4{1.0f, 0.0f, 0.0f, 1.0f};

      const auto noise = sbx::math::noise::fractal(center_position / radius * 1.1, 3u);

      const auto encoded_uv = sbx::math::vector2{0.0f, noise};

      const auto center_index = static_cast<std::uint32_t>(vertices.size());
    
      vertices.emplace_back(center_position, normal, encoded_uv, tangent);

      // Build local tangent space for CCW sorting
      const auto up = normal;
      const auto right = sbx::math::vector3::normalized(sbx::math::vector3::cross(sbx::math::vector3::up, up));
      const auto forward = sbx::math::vector3::cross(up, right);

      auto sorted = std::vector<corner>{};

      for (auto face_idx : face_indices) {
        const auto relative = face_centroids[face_idx] - center_position;
        const auto x = sbx::math::vector3::dot(relative, right);
        const auto y = sbx::math::vector3::dot(relative, forward);
        const auto angle = std::atan2(y, x);

        sorted.push_back({face_idx, angle});
      }

      std::sort(sorted.begin(), sorted.end(), [](const corner& a, const corner& b) {
        return a.angle < b.angle;
      });

      auto corner_indices = std::vector<std::uint32_t>{};

      for (const auto& [face_idx, _] : sorted) {
        const auto position = face_centroids[face_idx];
        const auto uv = encoded_uv;

        vertices.emplace_back(position, sbx::math::vector3::normalized(position), uv, tangent);
        corner_indices.push_back(static_cast<std::uint32_t>(vertices.size() - 1));
      }

      for (auto i = 0u; i < corner_indices.size(); ++i) {
        const auto a = center_index;
        const auto b = corner_indices[i];
        const auto c = corner_indices[(i + 1) % corner_indices.size()];

        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
      }
    }

    return std::make_unique<sbx::models::mesh>(std::move(vertices), std::move(indices), sbx::math::volume{sbx::math::vector3(-radius), sbx::math::vector3(radius)});
  }


  sbx::math::uuid _mesh_id;
  sbx::math::uuid _planet_id;
  sbx::graphics::image2d_handle _texture_id;
  sbx::graphics::image2d_handle _normal_texture_id;
  sbx::scenes::node _node;

}; // class terrain_module

} // namespace demo

#endif // DEMO_TERRAIN_TERRAIN_MODULE_HPP_
