#ifndef DEMO_APPLICATION_HPP_
#define DEMO_APPLICATION_HPP_

#include <libsbx/units/units.hpp>
#include <libsbx/utility/utility.hpp>
#include <libsbx/io/io.hpp>
#include <libsbx/math/math.hpp>
#include <libsbx/memory/memory.hpp>
#include <libsbx/signals/signals.hpp>
#include <libsbx/ecs/ecs.hpp>
#include <libsbx/core/core.hpp>
#include <libsbx/devices/devices.hpp>
#include <libsbx/graphics/graphics.hpp>
#include <libsbx/models/models.hpp>
#include <libsbx/scenes/scenes.hpp>
#include <libsbx/assets/assets.hpp>
#include <libsbx/ui/ui.hpp>
#include <libsbx/physics/physics.hpp>
#include <libsbx/animations/animations.hpp>

#include <demo/terrain/mesh.hpp>

namespace demo {

// class tank {

// public:

//   tank(const sbx::scenes::transform& transform) {
//     using namespace sbx::math::literals;

//     auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
//     auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

//     auto& scene = scenes_module.scene();

//     scene.add_material<sbx::scenes::material>("bmp_body1", sbx::scenes::material_type::opaque, sbx::math::color::white(), 0.0f, 0.5f, 1.0f, scene.get_image("bmp_body1_albedo"), scene.get_image("bmp_body1_normal"));
//     scene.add_material<sbx::scenes::material>("bmp_body2", sbx::scenes::material_type::opaque, sbx::math::color::white(), 0.0f, 0.5f, 1.0f, scene.get_image("bmp_body2_albedo"), scene.get_image("bmp_body2_normal"));
//     scene.add_material<sbx::scenes::material>("bmp_tracks", sbx::scenes::material_type::opaque, sbx::math::color::white(), 0.0f, 0.5f, 1.0f, scene.get_image("bmp_tracks_albedo"), scene.get_image("bmp_tracks_normal"));

//     auto& tank_mesh = assets_module.get_asset<sbx::models::mesh>(scene.get_mesh("bmp"));

//     const auto selection_tag = sbx::scenes::selection_tag{};

//     _root = scene.create_node("Tank", sbx::scenes::transform{}, selection_tag);
//     scene.get_component<sbx::scenes::transform>(_root) = transform;

//     _turret = scene.create_child_node(_root, "Turret", sbx::scenes::transform{}, selection_tag);
//     scene.add_component<sbx::scenes::static_mesh>(_turret, scene.get_mesh("bmp"), std::vector<sbx::scenes::static_mesh::submesh>{{tank_mesh.submesh_index("turret"), scene.get_material("bmp_body1")}});

//     _gun_primary = scene.create_child_node(_turret, "GunPrimary", sbx::scenes::transform{}, selection_tag);
//     scene.add_component<sbx::scenes::static_mesh>(_gun_primary, scene.get_mesh("bmp"), std::vector<sbx::scenes::static_mesh::submesh>{{tank_mesh.submesh_index("gun_primary"), scene.get_material("bmp_body1")}});

//     _gun_secondary = scene.create_child_node(_turret, "GunSecondary", sbx::scenes::transform{}, selection_tag);
//     scene.add_component<sbx::scenes::static_mesh>(_gun_secondary, scene.get_mesh("bmp"), std::vector<sbx::scenes::static_mesh::submesh>{{tank_mesh.submesh_index("gun_secondary"), scene.get_material("bmp_body1")}});

//     _hull = scene.create_child_node(_root, "Hull", sbx::scenes::transform{}, selection_tag);
//     scene.add_component<sbx::scenes::static_mesh>(_hull, scene.get_mesh("bmp"), std::vector<sbx::scenes::static_mesh::submesh>{{tank_mesh.submesh_index("hull"), scene.get_material("bmp_body2")}});

//     _track_l = scene.create_child_node(_root, "TrackL", sbx::scenes::transform{}, selection_tag);
//     scene.add_component<sbx::scenes::static_mesh>(_track_l, scene.get_mesh("bmp"), std::vector<sbx::scenes::static_mesh::submesh>{{tank_mesh.submesh_index("track_l"), scene.get_material("bmp_tracks")}});

//     _track_r = scene.create_child_node(_root, "TrackR", sbx::scenes::transform{}, selection_tag);
//     scene.add_component<sbx::scenes::static_mesh>(_track_r, scene.get_mesh("bmp"), std::vector<sbx::scenes::static_mesh::submesh>{{tank_mesh.submesh_index("track_r"), scene.get_material("bmp_tracks")}});

//     // for (auto i = 1u; i <= 6u; ++i) {
//     //   const auto name = fmt::format("WheelR{}", i);
//     //   const auto tag = fmt::format("wheel_r_{:02}", i);

//     //   auto& wheel_node = _wheels_r[i];

//     //   wheel_node = scene.create_child_node(_track_r, name, sbx::scenes::transform{tank_mesh.submesh_local_transform(tag) * sbx::math::vector4{0, 0, 0, 1}}, selection_tag);
//     //   scene.add_component<sbx::scenes::static_mesh>(wheel_node, scene.get_mesh("bmp"), tank_mesh.submesh_index(tag), sbx::math::color::white(), sbx::scenes::static_mesh::material{}, image_ids["bmp_body2_albedo"], image_ids["bmp_body2_normal"]);
//     // }

//     // _wheels_r[0u] = scene.create_child_node(_track_r, "IdlerR", sbx::scenes::transform{tank_mesh.submesh_local_transform("idler_r") * sbx::math::vector4{0, 0, 0, 1}}, selection_tag);
//     // scene.add_component<sbx::scenes::static_mesh>(_wheels_r[0u], scene.get_mesh("bmp"), tank_mesh.submesh_index("idler_r"), sbx::math::color::white(), sbx::scenes::static_mesh::material{}, image_ids["bmp_body2_albedo"], image_ids["bmp_body2_normal"]);

//     // _wheels_r[7u] = scene.create_child_node(_track_r, "SprocketR", sbx::scenes::transform{tank_mesh.submesh_local_transform("sprocket_r") * sbx::math::vector4{0, 0, 0, 1}}, selection_tag);
//     // scene.add_component<sbx::scenes::static_mesh>(_wheels_r[7u], scene.get_mesh("bmp"), tank_mesh.submesh_index("sprocket_r"), sbx::math::color::white(), sbx::scenes::static_mesh::material{}, image_ids["bmp_body2_albedo"], image_ids["bmp_body2_normal"]);

//     // _gun_primary_tip = scene.create_child_node(_gun_primary, "GunPrimaryTip", sbx::scenes::transform{sbx::math::vector3{0.9f, -0.1f, -10.5f}, sbx::math::quaternion::identity, sbx::math::vector3{0.5f, 0.5f, 0.5f}}, selection_tag);
//     // scene.add_component<sbx::scenes::static_mesh>(_gun_primary_tip, mesh_ids["cube"], 0u, sbx::math::color::red());
//   }

//   ~tank() {

//   }

//   auto update() {
//     // auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

//     // auto& scene = scenes_module.scene();

//     // const auto delta_time = sbx::core::engine::delta_time();

//     // auto& settings = sbx::core::engine::settings(); 

//     // auto gun_elevation_direction = 0;

//     // if (sbx::devices::input::is_key_down(sbx::devices::key::up)) {
//     //   gun_elevation_direction = 1;
//     // } else if (sbx::devices::input::is_key_down(sbx::devices::key::down)) {
//     //   gun_elevation_direction = -1;
//     // }

//     // _gun_elevation += sbx::math::degree{90} * delta_time * gun_elevation_direction;

//     // if (_gun_elevation > sbx::math::degree{45}) {
//     //   _gun_elevation = sbx::math::degree{45};
//     // }

//     // if (_gun_elevation < sbx::math::degree{-5}) {
//     //   _gun_elevation = sbx::math::degree{-5};
//     // }

//     // auto turret_rotation_direction = 0;

//     // if (sbx::devices::input::is_key_down(sbx::devices::key::left)) {
//     //   turret_rotation_direction = 1;
//     // } else if (sbx::devices::input::is_key_down(sbx::devices::key::right)) {
//     //   turret_rotation_direction = -1;
//     // }

//     // _turret_rotation += sbx::math::degree{90} * delta_time * turret_rotation_direction;

//     // auto& tank_root_transform = scene.get_component<sbx::scenes::transform>(_root);
    
//     // const auto forward = tank_root_transform.forward();
//     // const auto right = tank_root_transform.right();

//     // auto movement = sbx::math::vector3::zero;
//     // auto movement_direction = 0;

//     // if (sbx::devices::input::is_key_down(sbx::devices::key::o)) {
//     //   movement += forward;
//     //   movement_direction = 1;
//     // } 
    
//     // if (sbx::devices::input::is_key_down(sbx::devices::key::l)) {
//     //   movement -= forward;
//     //   movement_direction = -1;
//     // }

//     // _wheel_rotation -= sbx::math::degree{360} * sbx::math::vector3::normalized(movement).length() * delta_time * movement_direction;

//     // // tank_root_transform.move_by(sbx::math::vector3::normalized(movement) * delta_time * 12.0f);

//     // auto& tank_turret_transform = scene.get_component<sbx::scenes::transform>(_turret);
//     // tank_turret_transform.set_rotation(sbx::math::vector3::up, _turret_rotation);

//     // auto& tank_gun_primary_transform = scene.get_component<sbx::scenes::transform>(_gun_primary);
//     // tank_gun_primary_transform.set_rotation(sbx::math::vector3::right, _gun_elevation);

//     // auto& tank_gun_secondary_transform = scene.get_component<sbx::scenes::transform>(_gun_secondary);
//     // tank_gun_secondary_transform.set_rotation(sbx::math::vector3::right, _gun_elevation);

//     // auto& tank_track_r_static_mesh = scene.get_component<sbx::scenes::static_mesh>(_track_r);
//     // tank_track_r_static_mesh.submesh_at(0u).material.flexibility = -movement.length() * movement_direction * 3.0f;

//     // for (auto i : std::ranges::views::iota(0u, _wheels_r.size())) {
//     //   auto& wheel_transform = scene.get_component<sbx::scenes::transform>(_wheels_r[i]);
//     //   wheel_transform.set_rotation(sbx::math::vector3::right, _wheel_rotation);
//     // }
//   }

// private:

//   sbx::scenes::node _root;

//   sbx::scenes::node _hull;

//   sbx::scenes::node _turret;
//   sbx::scenes::node _gun_primary;
//   sbx::scenes::node _gun_primary_tip;
//   sbx::scenes::node _gun_secondary;

//   sbx::scenes::node _track_l;

//   sbx::scenes::node _track_r;
//   std::array<sbx::scenes::node, 8u> _wheels_r;

//   sbx::math::degree _turret_rotation;
//   sbx::math::degree _gun_elevation;
//   sbx::math::degree _wheel_rotation;

// }; // class tank

class application : public sbx::core::application {

public:

  application();

  ~application() override = default;

  auto update() -> void override;

  auto fixed_update() -> void override;

private:

  auto _generate_brdf(const std::uint32_t size) -> void;
  auto _generate_irradiance(const std::uint32_t size) -> void;
  auto _generate_prefiltered(const std::uint32_t size) -> void;

  // auto _generate_icosphere(const std::float_t radius, const std::uint32_t subdivisions) -> std::unique_ptr<sbx::models::mesh>;

  sbx::math::angle _rotation;

  sbx::scenes::node _player;

  sbx::graphics::storage_buffer_handle _selection_buffer;

  // std::vector<tank> _tanks;

  sbx::scenes::node _light_center;

  sbx::graphics::image2d_handle _brdf;
  sbx::graphics::cube_image2d_handle _irradiance;
  sbx::graphics::cube_image2d_handle _prefiltered;

}; // class application

} // namespace demo

#endif // DEMO_APPLICATION_HPP_
