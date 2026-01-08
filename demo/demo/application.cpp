#include <demo/application.hpp>

#include <nlohmann/json.hpp>

#include <easy/profiler.h>

#include <libsbx/math/color.hpp>
#include <libsbx/math/noise.hpp>
#include <libsbx/math/constants.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <demo/renderer.hpp>
#include <demo/line.hpp>

#include <demo/terrain/terrain_subrenderer.hpp>
#include <demo/terrain/terrain_module.hpp>
#include <demo/terrain/chunk.hpp>

#include <libsbx/devices/input.hpp>

#include <libsbx/scenes/debug_subrenderer.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/skinned_mesh.hpp>

#include <libsbx/scripting/scripting.hpp>

#include <libsbx/animations/mesh.hpp>
#include <libsbx/animations/animation.hpp>
#include <libsbx/animations/animator.hpp>

#include <libsbx/sprites/sprite_subrenderer.hpp>

namespace demo {

application::application()
: sbx::core::application{},
  _rotation{sbx::math::degree{0}} { 
  // Renderer
  const auto& cli = sbx::core::engine::cli();

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (auto assets = cli.argument<std::string>("assets"); assets) {
    assets_module.set_asset_root(*assets);
  } else {
    assets_module.set_asset_root("demo/assets");
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  graphics_module.set_renderer<renderer>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.load_scene("res://scenes/scene.yaml");

  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  // Textures

  scene.add_image("fox_albedo", "res://textures/fox/albedo.png", sbx::graphics::format::r8g8b8a8_srgb);

  scene.add_image("helmet_albedo", "res://textures/helmet/albedo.jpg", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("helmet_normal", "res://textures/helmet/normal.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  scene.add_image("helmet_mrao", "res://textures/helmet/mrao2.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  scene.add_image("helmet_emissive", "res://textures/helmet/emissive.jpg", sbx::graphics::format::r8g8b8a8_srgb);

  scene.add_image("bricks_albedo", "res://textures/bricks1/albedo.png", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("bricks_normal", "res://textures/bricks1/normal.png", sbx::graphics::format::r8g8b8a8_unorm);
  scene.add_image("bricks_mrao", "res://textures/bricks1/mrao.png", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("bricks_height", "res://textures/bricks1/height.png", sbx::graphics::format::r8g8b8a8_srgb);

  scene.add_image("duck_albedo", "res://textures/duck/albedo.png", sbx::graphics::format::r8g8b8a8_srgb);

  scene.add_image("checkerboard", "res://textures/checkerboard.jpg", sbx::graphics::format::r8g8b8a8_srgb);

  scene.add_image("chess_pieces_white_albedo", "res://textures/chess/pieces/white/albedo.jpg", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("chess_pieces_white_normal", "res://textures/chess/pieces/white/normal.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  scene.add_image("chess_pieces_white_mrao", "res://textures/chess/pieces/white/mrao.jpg", sbx::graphics::format::r8g8b8a8_srgb);

  scene.add_image("chess_pieces_black_albedo", "res://textures/chess/pieces/black/albedo.jpg", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("chess_pieces_black_normal", "res://textures/chess/pieces/black/normal.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  scene.add_image("chess_pieces_black_mrao", "res://textures/chess/pieces/black/mrao.jpg", sbx::graphics::format::r8g8b8a8_srgb);

  // scene.add_image("pine_tree_bark_albedo", "res://textures/pine_tree/bark_albedo.png", sbx::graphics::format::r8g8b8a8_srgb);
  // scene.add_image("pine_tree_bark_normal", "res://textures/pine_tree/bark_normal.png", sbx::graphics::format::r8g8b8a8_unorm);
  // scene.add_image("pine_tree_leaves_albedo", "res://textures/pine_tree/leaves_albedo.png", sbx::graphics::format::r8g8b8a8_srgb);
  // scene.add_image("pine_tree_leaves_normal", "res://textures/pine_tree/leaves_normal.png", sbx::graphics::format::r8g8b8a8_unorm);

  scene.add_image("tree_1_bark", "res://textures/tree_1/bark.jpg", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("tree_1_leaves1", "res://textures/tree_1/leaves1.png", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("tree_1_leaves2", "res://textures/tree_1/leaves2.png", sbx::graphics::format::r8g8b8a8_srgb);

  scene.add_cube_image("skybox", "res://skyboxes/clouds2");

  _generate_brdf(512);
  _generate_irradiance(64);
  _generate_prefiltered(512);

  // Meshes

  scene.add_mesh<sbx::animations::mesh>("fox", "res://meshes/fox/fox.gltf");

  scene.add_mesh<sbx::models::mesh>("helmet", "res://meshes/helmet/helmet.gltf");

  scene.add_mesh<sbx::models::mesh>("duck", "res://meshes/duck/duck.gltf");

  scene.add_mesh<sbx::models::mesh>("cube", "res://meshes/cube/cube.gltf");
  scene.add_mesh<sbx::models::mesh>("sphere", "res://meshes/sphere/sphere.gltf");

  scene.add_mesh<sbx::models::mesh>("chess_pieces", "res://meshes/chess/pieces.gltf");

  scene.add_mesh<sbx::models::mesh>("tree_1_1", "res://meshes/tree_1_2/tree_1_2.gltf");

  // Animations

  scene.add_animation<sbx::animations::animation>("Walk", "res://meshes/fox/fox.gltf", "Walk");
  scene.add_animation<sbx::animations::animation>("Survey", "res://meshes/fox/fox.gltf", "Survey");
  scene.add_animation<sbx::animations::animation>("Run", "res://meshes/fox/fox.gltf", "Run");

  // Window

  auto& devices_module = sbx::core::engine::get_module<sbx::devices::devices_module>();

  auto& window = devices_module.window();

  window.on_window_closed_signal() += [this]([[maybe_unused]] const auto& event){
    sbx::core::engine::quit();
  };

  // Terrain

  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  terrain_module.load_terrain_in_scene();

  // Circling point lights
  _light_center = scene.create_node("LightCenter", sbx::scenes::transform{sbx::math::vector3{0.0f, 20.0f, 0.0f}});
  
  const auto radius = 20.0f;
  const auto light_count = 8;
  
  for (auto i = 0; i < light_count; ++i) {
    auto angle = sbx::math::radian{2.0f * sbx::math::pi / static_cast<std::float_t>(light_count) * static_cast<std::float_t>(i)};

    // const auto material_name = fmt::format("Light{}", i);
    const auto color = sbx::math::random_color(0.8f);

    // auto& material = scene.add_material<sbx::models::material>(material_name);
    // material.base_color = color;
    // material.alpha = sbx::models::alpha_mode::blend;

    auto light = scene.create_child_node(_light_center, fmt::format("Light{}", i), sbx::scenes::transform{sbx::math::vector3{radius * sbx::math::cos(angle), 0.0f, radius * sbx::math::sin(angle)}});

    scene.add_component<sbx::scenes::point_light>(light, color, 50.0f);

    // scene.add_component<sbx::scenes::static_mesh>(light, scene.get_mesh("sphere"), scene.get_material(material_name));

    auto& light_transform = scene.get_component<sbx::scenes::transform>(light);
  }

  // Helmet
  _helmet = scene.create_node("Helmet", sbx::scenes::transform{});

  auto& helmet_material = scene.add_material<sbx::models::material>("helmet");
  helmet_material.albedo.image = scene.get_image("helmet_albedo");
  helmet_material.normal.image = scene.get_image("helmet_normal");
  helmet_material.normal_scale = 1.0f;
  helmet_material.mrao.image = scene.get_image("helmet_mrao");
  helmet_material.emissive.image = scene.get_image("helmet_emissive");
  helmet_material.emissive_factor = sbx::math::vector4{1, 1, 1, 0};
  helmet_material.emissive_strength = 16.0f;

  scene.add_component<sbx::scenes::static_mesh>(_helmet, scene.get_mesh("helmet"), scene.get_material("helmet"));

  auto& helmet_transform = scene.get_component<sbx::scenes::transform>(_helmet);
  helmet_transform.set_position(sbx::math::vector3{0.0f, 6.0f, 0.0f});
  helmet_transform.set_scale(sbx::math::vector3{4.0f, 4.0f, 4.0f});

  auto demo_script = scripting_module.instantiate(_helmet, "build/x86_64/gcc/debug/_dotnet/Demo.dll", "Demo.Helmet");

  demo_script.invoke("SayHello");

  auto sprite = scene.create_child_node(_helmet, "Sprite");

  auto& sprite_transform = scene.get_component<sbx::scenes::transform>(sprite);
  sprite_transform.set_position(sbx::math::vector3{0, 2, 0});
  sprite_transform.set_scale(sbx::math::vector3{0.1f, 0.25f, 0.1f});
  sprite_transform.set_rotation(sbx::math::vector3::right, sbx::math::degree{90});

  auto& sprite_sprite = scene.add_component<sbx::sprites::sprite>(sprite);
  sprite_sprite.space = sbx::sprites::sprite_space::screen_camera;
  sprite_sprite.image = scene.get_image("helmet_albedo");
  sprite_sprite.color = sbx::math::color::white();

  // Cube

  auto cube = scene.create_node("Cube");

  auto& cube_material = scene.add_material<sbx::models::material>("cube");
  cube_material.albedo.image = scene.get_image("helmet_albedo");
  cube_material.normal.image = scene.get_image("helmet_normal");
  cube_material.mrao.image = scene.get_image("helmet_mrao");
  cube_material.emissive.image = scene.get_image("helmet_emissive");
  cube_material.emissive_factor = sbx::math::vector4{1, 1, 1, 0};

  scene.add_component<sbx::scenes::static_mesh>(cube, scene.get_mesh("cube"), scene.get_material("cube"));

  auto& cube_transform = scene.get_component<sbx::scenes::transform>(cube);
  cube_transform.set_position(sbx::math::vector3{0.0f, 6.0f, 0.0f});
  cube_transform.set_scale(sbx::math::vector3{2.0f, 2.0f, 2.0f});

  auto& cube_collider = scene.add_component<sbx::physics::collider>(cube, sbx::physics::box{sbx::math::vector3{1, 1, 1}});

  auto& cube_rigidbody = scene.add_component<sbx::physics::rigidbody>(cube, sbx::units::kilogram{2});
  cube_rigidbody.set_inverse_inertia_tensor_local(local_inverse_inertia(cube_rigidbody.mass(), cube_collider));
  cube_rigidbody.add_constant_acceleration(sbx::math::vector3{0, -9.81f, 0});

  // Duck

  auto aaaa = scene.create_node("AAAA");

  _duck = scene.create_child_node(aaaa, "Duck");

  auto& duck_material = scene.add_material<sbx::models::material>("duck");
  duck_material.base_color = sbx::math::color{1.0f, 1.0f, 1.0f, 0.5f};
  duck_material.albedo.image = scene.get_image("duck_albedo");
  duck_material.metallic = 0.8f;
  duck_material.roughness = 0.2f;
  // duck_material.alpha = sbx::models::alpha_mode::blend;

  scene.add_component<sbx::scenes::static_mesh>(_duck, scene.get_mesh("duck"), std::vector<sbx::scenes::static_mesh::submesh>{{0u, scene.get_material("duck")}});

  auto& duck_transform = scene.get_component<sbx::scenes::transform>(_duck);
  duck_transform.set_position(sbx::math::vector3{-8.0f, 2.0f, 4.0f});
  duck_transform.set_rotation(sbx::math::vector3::up, sbx::math::degree{-45});
  duck_transform.set_scale(sbx::math::vector3{4.0f, 4.0f, 4.0f});

  // Cube2

  _cube2 = scene.create_node("Cube2");

  auto& cube2_material = scene.add_material<sbx::models::material>("cube2");
  cube2_material.albedo.image = scene.get_image("bricks_albedo");
  cube2_material.normal.image = scene.get_image("bricks_normal");
  // cube2_material.mrao.image = scene.get_image("bricks_mrao");
  cube2_material.metallic = 0.04f;
  cube2_material.roughness = 1.0f;
  cube2_material.occlusion = 1.0f;
  cube2_material.height.image = scene.get_image("bricks_height");
  cube2_material.normal_scale = 1.0f;

  scene.add_component<sbx::scenes::static_mesh>(_cube2, scene.get_mesh("cube"), scene.get_material("cube2"));

  auto& cube2_transform = scene.get_component<sbx::scenes::transform>(_cube2);
  cube2_transform.set_position(sbx::math::vector3{-8.0f, 15.0f, 4.0f});
  cube2_transform.set_scale(sbx::math::vector3{5.0f, 5.0f, 5.0f});
  cube2_transform.set_rotation(sbx::math::vector3{1.0f, 0.0f, 1.0f}, sbx::math::degree{30.0f});

  // Tree

  auto tree = scene.create_node("Tree");

  auto& tree_bark_material = scene.add_material<sbx::models::material>("tree_1_bark");
  tree_bark_material.albedo.image = scene.get_image("tree_1_bark");

  auto& tree_leaves1_material = scene.add_material<sbx::models::material>("tree_1_leaves1");
  tree_leaves1_material.albedo.image = scene.get_image("tree_1_leaves1");
  tree_leaves1_material.alpha = sbx::models::alpha_mode::mask;
  tree_leaves1_material.is_double_sided = true;

  auto& tree_leaves2_material = scene.add_material<sbx::models::material>("tree_1_leaves2");
  tree_leaves2_material.albedo.image = scene.get_image("tree_1_leaves2");
  tree_leaves2_material.alpha = sbx::models::alpha_mode::mask;
  tree_leaves2_material.is_double_sided = true;

  auto tree_submeshes = std::vector<sbx::scenes::static_mesh::submesh>{
    sbx::scenes::static_mesh::submesh{0, scene.get_material("tree_1_bark")}, 
    sbx::scenes::static_mesh::submesh{1, scene.get_material("tree_1_leaves1")},
    sbx::scenes::static_mesh::submesh{1, scene.get_material("tree_1_leaves2")}
  };

  scene.add_component<sbx::scenes::static_mesh>(tree, scene.get_mesh("tree_1_1"), tree_submeshes);

  auto& tree_transform = scene.get_component<sbx::scenes::transform>(tree);
  tree_transform.set_position(sbx::math::vector3{-20.0f, 0.0f, 4.0f});
  
  // Chess

  auto piece_names = std::array<std::string, 6>{
    "Pawn",
    "Rook",
    "Knight",
    "Bishop",
    "Queen",
    "King"
  };

  auto& chess_white_material = scene.add_material<sbx::models::material>("chess_white");
  chess_white_material.albedo.image = scene.get_image("chess_pieces_white_albedo");
  chess_white_material.normal.image = scene.get_image("chess_pieces_white_normal");
  // chess_white_material.mrao.image = scene.get_image("chess_pieces_white_mrao");
  chess_white_material.metallic = 0.1f;
  chess_white_material.roughness = 0.9f;
  chess_white_material.occlusion = 1.0f;

  auto& chess_black_material = scene.add_material<sbx::models::material>("chess_black");
  chess_black_material.albedo.image = scene.get_image("chess_pieces_black_albedo");
  chess_black_material.normal.image = scene.get_image("chess_pieces_black_normal");
  // chess_black_material.mrao.image = scene.get_image("chess_pieces_black_mrao");
  chess_black_material.metallic = 0.1f;
  chess_black_material.roughness = 0.9f;
  chess_black_material.occlusion = 1.0f;

  auto chess = scene.create_node("Chess");

  for (auto row = 0; row < 4; ++row) {
    for (auto i = 0; i < static_cast<int>(piece_names.size()); ++i) {
      const auto& piece_name = piece_names[i];

      auto white_piece = scene.create_child_node(chess, fmt::format("Chess_{}_White_{}", piece_name, row));

      scene.add_component<sbx::scenes::static_mesh>(white_piece, scene.get_mesh("chess_pieces"), std::vector<sbx::scenes::static_mesh::submesh>{{i, scene.get_material("chess_white")}});

      auto& white_piece_transform = scene.get_component<sbx::scenes::transform>(white_piece);
      white_piece_transform.set_position(sbx::math::vector3{static_cast<float>(i) * 2.0f - 5.0f, 0.7f, -10.0f - static_cast<float>(row) * 2.0f});
      white_piece_transform.set_scale(sbx::math::vector3{8.0f, 8.0f, 8.0f});

      auto black_piece = scene.create_child_node(chess, fmt::format("Chess_{}_Black_{}", piece_name, row));

      scene.add_component<sbx::scenes::static_mesh>(black_piece, scene.get_mesh("chess_pieces"), std::vector<sbx::scenes::static_mesh::submesh>{{i, scene.get_material("chess_black")}});

      auto& black_piece_transform = scene.get_component<sbx::scenes::transform>(black_piece);
      black_piece_transform.set_position(sbx::math::vector3{static_cast<float>(i) * 2.0f - 5.0f, 0.7f, -14.0f - static_cast<float>(row) * 2.0f});
      black_piece_transform.set_scale(sbx::math::vector3{8.0f, 8.0f, 8.0f});
    }
  }

  // Fox1

  auto& animations_module = sbx::core::engine::get_module<sbx::animations::animations_module>();

  auto& fox_material = scene.add_material<sbx::models::material>("fox");
  fox_material.albedo.image = scene.get_image("fox_albedo");
  fox_material.roughness = 0.7f;
  fox_material.occlusion = 0.8f;

  _fox1 = scene.create_node("Fox");

  animations_module.add_animated_mesh(_fox1, scene.get_mesh("fox"), scene.get_material("fox"));

  auto& fox1_animator = scene.add_component<sbx::animations::animator>(_fox1);
  fox1_animator.add_state({"Walk", scene.get_animation("Walk"), true, 1.5f});

  fox1_animator.play("Walk", true);

  auto& fox1_transform = scene.get_component<sbx::scenes::transform>(_fox1);
  fox1_transform.set_position(sbx::math::vector3{15.0f, 0.0f, 0.0f});
  fox1_transform.set_scale(sbx::math::vector3{0.06f, 0.06f, 0.06f});

  // Fox2

  _fox2 = scene.create_node("Fox");

  animations_module.add_animated_mesh(_fox2, scene.get_mesh("fox"), scene.get_material("fox"));

  auto fox_tail_node = animations_module.find_skeleton_node(_fox2, "b_Tail03_014");
  if (fox_tail_node != sbx::scenes::node::null) {
    auto test = scene.create_child_node(fox_tail_node, "Test");

    scene.add_component<sbx::scenes::static_mesh>(test, scene.get_mesh("sphere"), scene.get_material("fox"));

    auto& test_transform = scene.get_component<sbx::scenes::transform>(test);
    test_transform.set_scale(sbx::math::vector3{10.0f, 10.0f, 10.0f});
  }

  auto& fox2_animator = scene.add_component<sbx::animations::animator>(_fox2);

  fox2_animator.add_state({"Walk", scene.get_animation("Walk"), true, 0.5f});
  fox2_animator.add_state({"Survey", scene.get_animation("Survey"), true, 0.5f});
  fox2_animator.add_state({"Run", scene.get_animation("Run"), true, 0.5f});

  fox2_animator.set_float("speed", 0.0f);   // will be updated every frame

  fox2_animator.add_transition({
    "Walk", "Survey", 0.20f,
    [](const sbx::animations::animator& animator) -> bool {
      if (auto value = animator.float_parameter("speed"); value) {
        return *value <= 0.05f;
      }

      return false;
    }
  });

  fox2_animator.add_transition({
    "Run", "Survey", 0.25f,
    [](const sbx::animations::animator& animator) -> bool {
      if (auto value = animator.float_parameter("speed"); value) {
        return *value <= 0.05f;
      }

      return false;
    }
  });

  // Walk ↔ Run thresholds
  fox2_animator.add_transition({
    "Walk", "Run", 0.15f,
    [](const sbx::animations::animator& animator) -> bool {
      if (auto value = animator.float_parameter("speed"); value) {
        return *value >= 2.0f;
      }

      return false;
    }
  });

  fox2_animator.add_transition({
    "Run", "Walk", 0.15f,
    [](const sbx::animations::animator& animator) -> bool {
      if (auto value = animator.float_parameter("speed"); value) {
        return *value < 2.0f && *value > 0.05f;
      }

      return false;
    }
  });

  // Survey → Walk when starting to move
  fox2_animator.add_transition({
    "Survey", "Walk", 0.20f,
    [](const sbx::animations::animator& animator) -> bool {
      if (auto value = animator.float_parameter("speed"); value) {
        return *value > 0.05f && *value < 2.0f;
      }

      return false;
    }
  });

  fox2_animator.play("Survey", true);

  auto& fox2_transform = scene.get_component<sbx::scenes::transform>(_fox2);
  fox2_transform.set_position(sbx::math::vector3{12.0f, 0.0f, 0.0f});
  fox2_transform.set_scale(sbx::math::vector3{0.06f, 0.06f, 0.06f});

  auto spheres = scene.create_node(fmt::format("Spheres"));

  auto& spheres_transform = scene.get_component<sbx::scenes::transform>(spheres);
  spheres_transform.set_position(sbx::math::vector3{0, 0, -15});

  for (auto y = 0; y < 5; ++y) {
    for (auto x = 0; x < 5; ++x) {
      auto sphere = scene.create_child_node(spheres, fmt::format("Sphere{}{}", x, y));

      const auto material_name = fmt::format("sphere_{}_{}_material", x, y);

      auto& material = scene.add_material<sbx::models::material>(material_name);
      material.base_color = sbx::math::color::white();
      material.albedo.image = scene.get_image("checkerboard");
      material.metallic = 0.2f * x;
      material.roughness = 0.2f * y;
      material.occlusion = 1.0f;

      scene.add_component<sbx::scenes::static_mesh>(sphere, scene.get_mesh("sphere"), scene.get_material(material_name));

      auto& sphere_transform = scene.get_component<sbx::scenes::transform>(sphere);
      sphere_transform.set_position(sbx::math::vector3{x * 3, y * 3 + 5, 0.0f});
      sphere_transform.set_scale(sbx::math::vector3{1.0f, 1.0f, 1.0f});
    }
  }

  // Camera
  auto camera_node = scene.camera();
  auto camera_anchor = scene.create_node("CameraAnchor");

  scene.make_child_of(camera_node, camera_anchor);

  auto& camera_transform = scene.get_component<sbx::scenes::transform>(camera_node);
  camera_transform.set_position(sbx::math::vector3{0.0f, 50.0f, 50.0f});
  camera_transform.look_at(sbx::math::vector3::zero);

  scene.add_component<sbx::scenes::skybox>(camera_node, scene.get_cube_image("skybox"), _brdf, _irradiance, _prefiltered);

  auto camera_controller_script = scripting_module.instantiate(camera_anchor, "build/x86_64/gcc/debug/_dotnet/Demo.dll", "Demo.CameraController");

  if (auto hide_window = cli.argument<bool>("hide-window"); !hide_window) {
    window.show();
  }

  sbx::utility::logger<"demo">::info("string id: {}", sbx::utility::string_id<"foobar">());
}

auto application::update() -> void  {
  const auto delta_time = sbx::core::engine::delta_time();

  if (sbx::devices::input::is_key_pressed(sbx::devices::key::escape)) {
    sbx::core::engine::quit();
    return;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  auto& devices_module = sbx::core::engine::get_module<sbx::devices::devices_module>();
  auto& window = devices_module.window();

  if (scene.is_valid(_cube2)) {
    auto& cube2_transform = scene.get_component<sbx::scenes::transform>(_cube2);
    const auto& current_rotation = cube2_transform.rotation();
    const auto rotation = sbx::math::quaternion{sbx::math::vector3::up, sbx::math::angle{sbx::math::degree{30} * delta_time}};
    cube2_transform.set_rotation(rotation * current_rotation);
  }

  _rotation += sbx::math::degree{45} * delta_time;

  if (scene.is_valid(_light_center)) {
    auto& light_center_transform = scene.get_component<sbx::scenes::transform>(_light_center);
    light_center_transform.set_rotation(sbx::math::vector3::up, _rotation);
  }
}

auto application::fixed_update() -> void {

}

auto application::_generate_brdf(const std::uint32_t size) -> void {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.scene();

  _brdf = graphics_module.add_resource<sbx::graphics::image2d>(sbx::math::vector2u{size}, sbx::graphics::format::r16g16_sfloat, VK_IMAGE_LAYOUT_GENERAL);

  auto timer = sbx::utility::timer{};

  auto& brdf = graphics_module.get_resource<sbx::graphics::image2d>(_brdf);

  auto command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_COMPUTE_BIT};

  auto pipeline = sbx::graphics::compute_pipeline{"res://shaders/brdf"};

  pipeline.bind(command_buffer);

  auto descriptor_handler = sbx::graphics::descriptor_handler{pipeline, 0u};

  descriptor_handler.push("output", brdf);

  descriptor_handler.update(pipeline);
  descriptor_handler.bind_descriptors(command_buffer);

  const uint32_t group_count_x = (brdf.size().x() + threads_per_group - 1) / threads_per_group;
  const uint32_t group_count_y = (brdf.size().y() + threads_per_group - 1) / threads_per_group;

  pipeline.dispatch(command_buffer, sbx::math::vector3u{group_count_x, group_count_y, 1u});

  command_buffer.submit_idle();

  sbx::utility::logger<"application">::info("Generated 'brdf' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
}

auto application::_generate_irradiance(const std::uint32_t size) -> void {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.scene();

  _irradiance = graphics_module.add_resource<sbx::graphics::cube_image>(sbx::math::vector2u{size}, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL);

  auto timer = sbx::utility::timer{};

  auto& irradiance = graphics_module.get_resource<sbx::graphics::cube_image>(_irradiance);

  auto command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_COMPUTE_BIT};

  auto pipeline = sbx::graphics::compute_pipeline{"res://shaders/irradiance"};

  pipeline.bind(command_buffer);

  auto descriptor_handler = sbx::graphics::descriptor_handler{pipeline, 0u};

  descriptor_handler.push("skybox", graphics_module.get_resource<sbx::graphics::cube_image>(scene.get_cube_image("skybox")));
  descriptor_handler.push("output", irradiance);

  descriptor_handler.update(pipeline);
  descriptor_handler.bind_descriptors(command_buffer);

  // const auto group_count_x = static_cast<std::uint32_t>(std::ceil(static_cast<std::float_t>(irradiance.size().x()) / static_cast<std::float_t>(threads_per_group)));
  // const auto group_count_y = static_cast<std::uint32_t>(std::ceil(static_cast<std::float_t>(irradiance.size().y()) / static_cast<std::float_t>(threads_per_group)));
  const uint32_t group_count_x = (irradiance.size().x() + threads_per_group - 1) / threads_per_group;
  const uint32_t group_count_y = (irradiance.size().y() + threads_per_group - 1) / threads_per_group;

  pipeline.dispatch(command_buffer, sbx::math::vector3u{group_count_x, group_count_y, 1u});

  command_buffer.submit_idle();

  sbx::utility::logger<"application">::info("Generated 'irradiance' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
}

auto application::_generate_prefiltered(uint32_t size) -> void {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  _prefiltered = graphics_module.add_resource<sbx::graphics::cube_image>(sbx::math::vector2u{size}, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLE_COUNT_1_BIT, true, true);

  auto timer = sbx::utility::timer{};

  auto& prefiltered = graphics_module.get_resource<sbx::graphics::cube_image>(_prefiltered);

  auto command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_COMPUTE_BIT};

  auto pipeline = sbx::graphics::compute_pipeline{"res://shaders/prefiltered"};
  auto push_handler = sbx::graphics::push_handler{pipeline};

  pipeline.bind(command_buffer);

  auto descriptor_handlers = std::vector<sbx::graphics::descriptor_handler>{};
  descriptor_handlers.reserve(prefiltered.mip_levels());

  for (auto mip = 0; mip < prefiltered.mip_levels(); ++mip) {
    descriptor_handlers.emplace_back(pipeline, 0u);
  }

  auto mip_views = std::vector<VkImageView>{};
  mip_views.resize(prefiltered.mip_levels());

  for (auto mip = 0; mip < prefiltered.mip_levels(); ++mip) {
    sbx::graphics::image::create_image_view(prefiltered, mip_views[mip], VK_IMAGE_VIEW_TYPE_2D_ARRAY, prefiltered.format(), VK_IMAGE_ASPECT_COLOR_BIT, 1, mip, 6, 0);
  }

  auto& skybox = graphics_module.get_resource<sbx::graphics::cube_image>(scene.get_cube_image("skybox"));

  for (auto mip = 0; mip < prefiltered.mip_levels(); ++mip) {
    auto barrier = VkImageMemoryBarrier2{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = prefiltered;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = mip;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;

    auto dependency = VkDependencyInfo{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(command_buffer, &dependency);

    auto& descriptor_handler = descriptor_handlers[mip];

    auto descriptor_image_info = std::vector<VkDescriptorImageInfo>{};
    descriptor_image_info.reserve(1u);

    descriptor_image_info.push_back(VkDescriptorImageInfo{
      .sampler = VK_NULL_HANDLE, // prefiltered.sampler(),
      .imageView = mip_views[mip],
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    });

    auto descriptor_write = VkWriteDescriptorSet{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstBinding = *pipeline.find_descriptor_binding("output", 0u);
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = *pipeline.find_descriptor_type_at_binding(0u, descriptor_write.dstBinding);

    auto write_set = sbx::graphics::write_descriptor_set{descriptor_write, descriptor_image_info};

    descriptor_handler.push("skybox", skybox);
    descriptor_handler.push("output", prefiltered, std::move(write_set));

    descriptor_handler.update(pipeline);
    descriptor_handler.bind_descriptors(command_buffer);

    const auto roughness = static_cast<std::float_t>(mip) / static_cast<std::float_t>(prefiltered.mip_levels() - 1);

    push_handler.push("roughness", roughness);
    // push_handler.push("num_samples", 32u);
    push_handler.bind(command_buffer);

    const uint32_t group_count_x = ((prefiltered.size().x() >> mip) + threads_per_group - 1) / threads_per_group;
    const uint32_t group_count_y = ((prefiltered.size().y() >> mip) + threads_per_group - 1) / threads_per_group;

    pipeline.dispatch(command_buffer, {group_count_x, group_count_y, 1});
  }

  auto barrier = VkImageMemoryBarrier2{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.image = prefiltered;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = prefiltered.mip_levels();
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 6;

  auto dependency = VkDependencyInfo{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &barrier;

  vkCmdPipelineBarrier2(command_buffer, &dependency);

  command_buffer.submit_idle();

  for (auto& view : mip_views) {
    vkDestroyImageView(graphics_module.logical_device(), view, nullptr);
  }

  sbx::utility::logger<"application">::info("Generated 'prefiltered' with {} mips in {:.2f}ms", prefiltered.mip_levels(), sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
}


} // namespace demo
