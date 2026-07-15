// SPDX-License-Identifier: MIT
#include <editor/application.hpp>

#include <utility>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/reflection/reflection.hpp>

#include <libsbx/math/color.hpp>
#include <libsbx/math/noise.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/devices/input.hpp>

#include <libsbx/scenes/debug_subrenderer.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/skinned_mesh.hpp>
#include <libsbx/scenes/components/skybox.hpp>

#include <libsbx/scripting/scripting.hpp>

#include <libsbx/animations/mesh.hpp>
#include <libsbx/animations/animation.hpp>
#include <libsbx/animations/animator.hpp>
#include <libsbx/animations/animations_module.hpp>

#include <libsbx/sprites/sprite_subrenderer.hpp>

#include <libsbx/particles/particle_emitter.hpp>

#include <libsbx/physics/physics_module.hpp>
#include <libsbx/physics/mesh_collider.hpp>
#include <libsbx/physics/shape_collider.hpp>
#include <libsbx/physics/rigidbody.hpp>

#include <libsbx/ui/ui_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>
#include <libsbx/filesystem/native_filesystem.hpp>

#include <libsbx/audio/audio_module.hpp>
#include <libsbx/sprites/sprites_module.hpp>

#include <libsbx/graphics/texture.hpp>

#include <libsbx/models/mesh.hpp>
#include <libsbx/models/material.hpp>

#include <editor/renderer.hpp>

namespace editor {

application::application()
: sbx::core::application{},
  _is_paused{false} {
  // Renderer
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  assets_module.set_asset_root("editor/assets");

  auto& filesystem_module = sbx::core::engine::get_module<sbx::filesystem::filesystem_module>();

  const auto engine_data_dir = filesystem_module.native_path_of(std::string{"engine://"});
  const auto editor_data_dir = engine_data_dir / "editor";

  if (!std::filesystem::exists(editor_data_dir)) {
    std::filesystem::create_directories(editor_data_dir);
  }

  filesystem_module.create_filesystem<sbx::filesystem::native_filesystem>(sbx::filesystem::alias{"editor://"}, editor_data_dir.generic_string());

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  graphics_module.set_renderer<editor::renderer>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.create_scene("Scene");

  scenes_module.set_scene_viewport("scene");

  auto& graph = scene.graph();
  auto& environment = scene.environment();

  // Asset helpers

  auto bind_texture = [&assets_module](sbx::models::material& material, sbx::models::texture_slot& slot, const std::string& path, const bool is_srgb) -> void {
    auto settings = YAML::Node{};

    settings["srgb"] = is_srgb;

    const auto id = assets_module.load_asset(path, settings);

    slot.image = assets_module.get_loaded<sbx::graphics::texture>(id).handle();
    material.texture_dependencies.push_back(id);
  };

  // Shared meshes

  _cube_mesh = assets_module.load_asset("res://meshes/cube/cube.gltf");

  const auto sphere_mesh = assets_module.load_asset("res://meshes/sphere/sphere.gltf");
  const auto helmet_mesh = assets_module.load_asset("res://meshes/helmet/helmet.gltf");
  const auto tree_mesh = assets_module.load_asset("res://meshes/tree/tree.gltf");
  const auto pawn_mesh = assets_module.load_asset("res://meshes/chess/pawn/pawn.gltf");

  const auto fox_mesh = assets_module.load_asset("res://meshes/chess/pawn/pawn.gltf");

  // Spheres

  auto spheres = graph.create_node(fmt::format("Spheres"));

  auto& spheres_transform = graph.get_component<sbx::scenes::transform>(spheres);
  spheres_transform.set_position(sbx::math::vector3{0, 0, -15});

  for (auto y = 0; y < 5; ++y) {
    for (auto x = 0; x < 5; ++x) {
      auto sphere = graph.create_child_node(spheres, fmt::format("Sphere{}{}", x, y));

      auto material = sbx::models::material{};
      material.base_color = sbx::math::color::white();
      material.alpha = sbx::models::alpha_mode::opaque;
      material.metallic_factor = 0.2f * x;
      material.roughness_factor = 0.2f * y;
      material.occlusion_strength = 1.0f;

      const auto material_id = assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(material)));

      graph.add_component<sbx::scenes::static_mesh>(sphere, sphere_mesh, material_id);

      auto& sphere_transform = graph.get_component<sbx::scenes::transform>(sphere);
      sphere_transform.set_position(sbx::math::vector3{x * 3, y * 3 + 5, 0.0f});
      sphere_transform.set_scale(sbx::math::vector3{1.0f, 1.0f, 1.0f});
    }
  }

  // Helmet

  auto helmet = graph.create_node("Helmet");

  auto& helmet_transform = graph.get_component<sbx::scenes::transform>(helmet);
  helmet_transform.set_position(sbx::math::vector3{0.0f, 5.0f, 0.0f});
  helmet_transform.set_scale(sbx::math::vector3{1.0f, 1.0f, 1.0f});

  auto helmet_material = sbx::models::material{};
  helmet_material.base_color = sbx::math::color::white();
  helmet_material.alpha = sbx::models::alpha_mode::opaque;
  helmet_material.metallic_factor = 1.0f;
  helmet_material.roughness_factor = 1.0f;
  helmet_material.occlusion_strength = 1.0f;
  helmet_material.emissive_factor = sbx::math::vector4{1.0f, 1.0f, 1.0f, 1.0f};
  helmet_material.emissive_strength = 5.0f;

  bind_texture(helmet_material, helmet_material.albedo, "res://meshes/helmet/textures/albedo.jpg", true);
  bind_texture(helmet_material, helmet_material.normal, "res://meshes/helmet/textures/normal.jpg", false);
  bind_texture(helmet_material, helmet_material.metallic_roughness, "res://meshes/helmet/textures/mr.jpg", false);
  bind_texture(helmet_material, helmet_material.occlusion, "res://meshes/helmet/textures/ao.jpg", false);
  bind_texture(helmet_material, helmet_material.emissive, "res://meshes/helmet/textures/emissive.jpg", true);

  graph.add_component<sbx::scenes::static_mesh>(helmet, helmet_mesh, assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(helmet_material))));

  // Transparency

  auto red = graph.create_node("Red");

  auto& red_transform = graph.get_component<sbx::scenes::transform>(red);
  red_transform.set_position(sbx::math::vector3{-5.0f, 5.0f, 0.0f});
  red_transform.set_scale(sbx::math::vector3{2.0f, 2.0f, 2.0f});

  auto red_material = sbx::models::material{};
  red_material.base_color = sbx::math::color{1.0f, 0.0f, 0.0f, 0.7f};
  red_material.alpha = sbx::models::alpha_mode::blend;

  graph.add_component<sbx::scenes::static_mesh>(red, _cube_mesh, assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(red_material))));

  auto green = graph.create_node("Green");

  auto& green_transform = graph.get_component<sbx::scenes::transform>(green);
  green_transform.set_position(sbx::math::vector3{-5.0f, 5.0f, 2.5f});
  green_transform.set_scale(sbx::math::vector3{2.0f, 2.0f, 2.0f});

  auto green_material = sbx::models::material{};
  green_material.base_color = sbx::math::color{0.0f, 1.0f, 0.0f, 0.7f};
  green_material.alpha = sbx::models::alpha_mode::blend;

  graph.add_component<sbx::scenes::static_mesh>(green, _cube_mesh, assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(green_material))));

  auto blue = graph.create_node("Blue");

  auto& blue_transform = graph.get_component<sbx::scenes::transform>(blue);
  blue_transform.set_position(sbx::math::vector3{-5.0f, 5.0f, -2.5f});
  blue_transform.set_scale(sbx::math::vector3{2.0f, 2.0f, 2.0f});

  auto blue_material = sbx::models::material{};
  blue_material.base_color = sbx::math::color{0.0f, 0.0f, 1.0f, 0.7f};
  blue_material.alpha = sbx::models::alpha_mode::blend;

  graph.add_component<sbx::scenes::static_mesh>(blue, _cube_mesh, assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(blue_material))));

  // Base

  auto base = graph.create_node("Base");

  auto& base_transform = graph.get_component<sbx::scenes::transform>(base);
  base_transform.set_position(sbx::math::vector3{0.0f, 0.0f, 0.0f});
  base_transform.set_scale(sbx::math::vector3{100.0f, 0.2f, 100.0f});

  auto base_material = sbx::models::material{};

  bind_texture(base_material, base_material.albedo, "res://textures/floor/albedo.png", true);
  base_material.albedo.anisotropy = 16.0f;
  bind_texture(base_material, base_material.normal, "res://textures/floor/normal.png", false);
  base_material.normal.anisotropy = 16.0f;
  bind_texture(base_material, base_material.metallic_roughness, "res://textures/floor/arm.png", false);
  base_material.metallic_roughness.anisotropy = 16.0f;
  bind_texture(base_material, base_material.occlusion, "res://textures/floor/ao.png", false);
  base_material.occlusion.anisotropy = 16.0f;
  bind_texture(base_material, base_material.height, "res://textures/floor/height.png", false);
  base_material.height.anisotropy = 16.0f;

  base_material.uv0_scale = sbx::math::vector2{15, 15};
  base_material.alpha = sbx::models::alpha_mode::opaque;
  base_material.metallic_factor = 0.0f;
  base_material.roughness_factor = 1.0f;
  base_material.occlusion_strength = 1.0f;
  base_material.specular_factor = 0.0f;

  graph.add_component<sbx::scenes::static_mesh>(base, _cube_mesh, assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(base_material))));

  auto& base_rigidbody = graph.add_component<sbx::physics::rigidbody>(base);
  base_rigidbody.set_is_static(true);

  graph.add_component<sbx::physics::shape_collider>(base, sbx::physics::box{sbx::math::vector3{50.0f, 0.1f, 50.0f}});

  // Cube material reused for runtime-spawned cubes

  auto cube_material = sbx::models::material{};
  cube_material.base_color = sbx::math::color::white();
  cube_material.alpha = sbx::models::alpha_mode::opaque;
  cube_material.metallic_factor = 0.0f;
  cube_material.roughness_factor = 1.0f;
  cube_material.occlusion_strength = 1.0f;
  cube_material.specular_factor = 0.0f;

  _cube_material = assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(cube_material)));

  // Tree

  auto tree = graph.create_node("Tree");

  auto& tree_transform = graph.get_component<sbx::scenes::transform>(tree);
  tree_transform.set_position(sbx::math::vector3{2.0f, 0.0f, 2.0f});

  auto tree_bark_material = sbx::models::material{};
  bind_texture(tree_bark_material, tree_bark_material.albedo, "res://meshes/tree/textures/bark/albedo.png", true);
  bind_texture(tree_bark_material, tree_bark_material.normal, "res://meshes/tree/textures/bark/normal.png", false);
  bind_texture(tree_bark_material, tree_bark_material.metallic_roughness, "res://meshes/tree/textures/bark/arm.png", false);
  tree_bark_material.alpha = sbx::models::alpha_mode::opaque;
  tree_bark_material.metallic_factor = 0.0f;
  tree_bark_material.roughness_factor = 1.0f;
  tree_bark_material.occlusion_strength = 1.0f;
  tree_bark_material.specular_factor = 0.0f;
  tree_bark_material.sway_speed = 0.8f;
  tree_bark_material.sway_strength = 0.04f;
  tree_bark_material.sway_falloff_exponent = 3.0f;

  auto tree_leaves_material = sbx::models::material{};
  bind_texture(tree_leaves_material, tree_leaves_material.albedo, "res://meshes/tree/textures/leaves/albedo.png", true);
  bind_texture(tree_leaves_material, tree_leaves_material.normal, "res://meshes/tree/textures/leaves/normal.png", false);
  bind_texture(tree_leaves_material, tree_leaves_material.metallic_roughness, "res://meshes/tree/textures/leaves/arm.png", false);
  tree_leaves_material.alpha = sbx::models::alpha_mode::mask;
  tree_leaves_material.is_double_sided = true;
  tree_leaves_material.alpha_cutoff = 0.5f;
  tree_leaves_material.metallic_factor = 0.0f;
  tree_leaves_material.roughness_factor = 1.0f;
  tree_leaves_material.occlusion_strength = 1.0f;
  tree_leaves_material.specular_factor = 0.0f;
  tree_leaves_material.sway_speed = 1.0f;
  tree_leaves_material.sway_strength = 0.06f;
  tree_leaves_material.sway_falloff_exponent = 2.0f;
  tree_leaves_material.scrumble_speed = 3.0f;
  tree_leaves_material.scrumble_strength = 0.02f;
  tree_leaves_material.scrumble_falloff_exponent = 1.5f;

  auto tree_submeshes = std::vector<sbx::scenes::static_mesh::submesh>{
    sbx::scenes::static_mesh::submesh{0, assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(tree_bark_material)))},
    sbx::scenes::static_mesh::submesh{1, assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(tree_leaves_material)))}
  };

  graph.add_component<sbx::scenes::static_mesh>(tree, tree_mesh, tree_submeshes);

  // Chess

  auto black_material = sbx::models::material{};
  bind_texture(black_material, black_material.albedo, "res://textures/chess/black/albedo.png", true);
  bind_texture(black_material, black_material.normal, "res://textures/chess/black/normal.png", false);
  bind_texture(black_material, black_material.metallic_roughness, "res://textures/chess/black/metallic_roughness.png", false);
  black_material.alpha = sbx::models::alpha_mode::opaque;
  black_material.metallic_factor = 0.0f;
  black_material.roughness_factor = 1.0f;
  black_material.occlusion_strength = 1.0f;
  black_material.specular_factor = 0.0f;

  auto white_material = sbx::models::material{};
  bind_texture(white_material, white_material.albedo, "res://textures/chess/white/albedo.png", true);
  bind_texture(white_material, white_material.normal, "res://textures/chess/white/normal.png", false);
  bind_texture(white_material, white_material.metallic_roughness, "res://textures/chess/white/metallic_roughness.png", false);
  white_material.alpha = sbx::models::alpha_mode::opaque;
  white_material.metallic_factor = 0.0f;
  white_material.roughness_factor = 1.0f;
  white_material.occlusion_strength = 1.0f;
  white_material.specular_factor = 0.0f;

  const auto black_material_id = assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(black_material)));
  const auto white_material_id = assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(white_material)));

  for (auto i = 0; i < 8; ++i) {
    auto white_pawn = graph.create_node(fmt::format("White Pawn {}", i + 1));

    auto& white_pawn_transform = graph.get_component<sbx::scenes::transform>(white_pawn);
    white_pawn_transform.set_position(sbx::math::vector3{-3.5f + i, 0.1f, -3.0f});

    graph.add_component<sbx::scenes::static_mesh>(white_pawn, pawn_mesh, white_material_id);

    auto black_pawn = graph.create_node(fmt::format("Black Pawn {}", i + 1));

    auto& black_pawn_transform = graph.get_component<sbx::scenes::transform>(black_pawn);
    black_pawn_transform.set_position(sbx::math::vector3{-3.5f + i, 0.1f, 3.0f});

    graph.add_component<sbx::scenes::static_mesh>(black_pawn, pawn_mesh, black_material_id);
  }

  // Camera

  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  const auto dotnet_dir = filesystem_module.native_path_of(std::string{"engine://dotnet"});

  auto core_assembly_path = std::filesystem::path{dotnet_dir / "editor/Editor.dll"};

  scripting_module.load_assembly(core_assembly_path.string());

  auto camera_node = environment.camera();

  auto& camera_transform = graph.get_component<sbx::scenes::transform>(camera_node);
  camera_transform.set_position(sbx::math::vector3{2.0f, 7.0f, 2.0f});
  camera_transform.look_at(helmet_transform.position());

  auto& skybox = graph.add_component<sbx::scenes::skybox>(camera_node);
  skybox.environment = assets_module.load_asset("res://skyboxes/hdr/clouds.envmap.yaml");

  scripting_module.instantiate(camera_node, "Editor.CameraController");

  auto& devices_module = sbx::core::engine::get_module<sbx::devices::devices_module>();

  auto& window = devices_module.window();

  window.set_title(fmt::format("SBX Editor [{}]", scene.name()));

  window.on_window_closed_signal() += [this]([[maybe_unused]] const auto& event){
    sbx::core::engine::quit();
  };
}

application::~application() {

}

auto application::update() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto& graph = scene.graph();

  if (sbx::devices::input::is_key_pressed(sbx::devices::key::space)) {
    auto cube = graph.create_node("Cube");

    auto axis = sbx::math::vector3::normalized(sbx::math::vector3{sbx::math::random::next<std::float_t>(-1.0f, 1.0f), sbx::math::random::next<std::float_t>(-1.0f, 1.0f), sbx::math::random::next<std::float_t>(-1.0f, 1.0f)});
    auto angle = sbx::math::angle{sbx::math::radian{sbx::math::random::next<std::float_t>(sbx::math::radian::min, sbx::math::radian::max)}};

    auto& cube_transform = graph.get_component<sbx::scenes::transform>(cube);
    cube_transform.set_position(sbx::math::vector3{0.0f, 10.0f, 0.0f});
    cube_transform.set_scale(sbx::math::vector3{1.0f, 1.0f, 1.0f});
    cube_transform.set_rotation(axis, angle);

    graph.add_component<sbx::scenes::static_mesh>(cube, _cube_mesh, _cube_material);

    auto& cube_collider = graph.add_component<sbx::physics::shape_collider>(cube, sbx::physics::box{sbx::math::vector3{0.5f, 0.5f, 0.5f}});

    auto& cube_rigidbody = graph.add_component<sbx::physics::rigidbody>(cube);
    cube_rigidbody.set_mass(1.0f);
    cube_rigidbody.add_constant_acceleration(sbx::math::vector3{0.0f, -9.81f, 0.0f});
    cube_rigidbody.set_inverse_inertia_tensor(sbx::physics::inverse_inertia_tensor(cube_collider, cube_rigidbody.mass()));
  }
}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

} // namespace editor
