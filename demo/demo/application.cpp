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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define DUMP_IMAGES 0

namespace demo {

static auto fox1 = sbx::scenes::node{};
static auto fox2 = sbx::scenes::node{};

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

    const auto material_name = fmt::format("Light{}", i);
    const auto color = sbx::math::random_color(0.8f);

    auto& material = scene.add_material<sbx::models::material>(material_name);
    material.base_color = color;
    material.alpha = sbx::models::alpha_mode::blend;

    auto light = scene.create_child_node(_light_center, fmt::format("Light{}", i), sbx::scenes::transform{sbx::math::vector3{radius * sbx::math::cos(angle), 0.0f, radius * sbx::math::sin(angle)}});

    scene.add_component<sbx::scenes::point_light>(light, color, 50.0f);

    scene.add_component<sbx::scenes::static_mesh>(light, scene.get_mesh("sphere"), scene.get_material(material_name));

    auto& light_transform = scene.get_component<sbx::scenes::transform>(light);
  }

  // Helmet
  auto helmet = scene.create_node("Helmet");

  auto& helmet_material = scene.add_material<sbx::models::material>("helmet");
  helmet_material.albedo.image = scene.get_image("helmet_albedo");
  helmet_material.normal.image = scene.get_image("helmet_normal");
  helmet_material.normal_scale = 1.0f;
  helmet_material.mrao.image = scene.get_image("helmet_mrao");
  helmet_material.emissive.image = scene.get_image("helmet_emissive");
  helmet_material.emissive_factor = sbx::math::vector4{1, 1, 1, 0};
  helmet_material.emissive_strength = 16.0f;

  scene.add_component<sbx::scenes::static_mesh>(helmet, scene.get_mesh("helmet"), scene.get_material("helmet"));

  auto& helmet_transform = scene.get_component<sbx::scenes::transform>(helmet);
  helmet_transform.set_position(sbx::math::vector3{0.0f, 6.0f, 0.0f});
  helmet_transform.set_scale(sbx::math::vector3{4.0f, 4.0f, 4.0f});

  auto demo_script = scripting_module.instantiate(helmet, "build/x86_64/gcc/debug/_dotnet/Demo.dll", "Demo.Helmet");

  demo_script.invoke("SayHello");

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

  auto duck = scene.create_node("Duck");

  auto& duck_material = scene.add_material<sbx::models::material>("duck");
  duck_material.base_color = sbx::math::color{1.0f, 1.0f, 1.0f, 0.5f};
  duck_material.albedo.image = scene.get_image("duck_albedo");
  duck_material.metallic = 0.3f;
  duck_material.roughness = 0.8f;
  // duck_material.alpha = sbx::models::alpha_mode::blend;

  scene.add_component<sbx::scenes::static_mesh>(duck, scene.get_mesh("duck"), std::vector<sbx::scenes::static_mesh::submesh>{{0u, scene.get_material("duck")}});

  auto& duck_transform = scene.get_component<sbx::scenes::transform>(duck);
  duck_transform.set_position(sbx::math::vector3{-8.0f, 2.0f, 4.0f});
  duck_transform.set_rotation(sbx::math::vector3::up, sbx::math::degree{-45});
  duck_transform.set_scale(sbx::math::vector3{4.0f, 4.0f, 4.0f});

  // Cube2

  auto cube2 = scene.create_node("Cube2");

  auto& cube2_material = scene.add_material<sbx::models::material>("cube2");
  cube2_material.albedo.image = scene.get_image("bricks_albedo");
  cube2_material.normal.image = scene.get_image("bricks_normal");
  // cube2_material.mrao.image = scene.get_image("bricks_mrao");
  cube2_material.metallic = 0.04f;
  cube2_material.roughness = 1.0f;
  cube2_material.occlusion = 1.0f;
  cube2_material.height.image = scene.get_image("bricks_height");
  cube2_material.normal_scale = 1.0f;

  scene.add_component<sbx::scenes::static_mesh>(cube2, scene.get_mesh("cube"), scene.get_material("cube2"));

  auto& cube2_transform = scene.get_component<sbx::scenes::transform>(cube2);
  cube2_transform.set_position(sbx::math::vector3{-8.0f, 15.0f, 4.0f});
  cube2_transform.set_scale(sbx::math::vector3{5.0f, 5.0f, 5.0f});
  
  // Chess

  auto piece_names = std::array<std::string, 6>{
    "Pawn",
    "Rook",
    "Knight",
    "Bishop",
    "Queen",
    "King"
  };

  auto& chess_pawn_white_material = scene.add_material<sbx::models::material>("chess_pawn_white");
  chess_pawn_white_material.albedo.image = scene.get_image("chess_pieces_white_albedo");
  chess_pawn_white_material.normal.image = scene.get_image("chess_pieces_white_normal");
  // chess_pawn_white_material.mrao.image = scene.get_image("chess_pieces_white_mrao");
  chess_pawn_white_material.metallic = 0.1f;
  chess_pawn_white_material.roughness = 0.9f;
  chess_pawn_white_material.occlusion = 1.0f;

  auto& chess_pawn_black_material = scene.add_material<sbx::models::material>("chess_pawn_black");
  chess_pawn_black_material.albedo.image = scene.get_image("chess_pieces_black_albedo");
  chess_pawn_black_material.normal.image = scene.get_image("chess_pieces_black_normal");
  // chess_pawn_black_material.mrao.image = scene.get_image("chess_pieces_black_mrao");
  chess_pawn_black_material.metallic = 0.1f;
  chess_pawn_black_material.roughness = 0.9f;
  chess_pawn_black_material.occlusion = 1.0f;

  // create a 4 rows of 2x white and 4 rows of 2x black pieces

  for (auto row = 0; row < 4; ++row) {
    for (auto i = 0; i < static_cast<int>(piece_names.size()); ++i) {
      const auto& piece_name = piece_names[i];

      auto white_piece = scene.create_node(fmt::format("Chess_{}_White_{}", piece_name, row));

      scene.add_component<sbx::scenes::static_mesh>(white_piece, scene.get_mesh("chess_pieces"), std::vector<sbx::scenes::static_mesh::submesh>{{i, scene.get_material("chess_pawn_white")}});

      auto& white_piece_transform = scene.get_component<sbx::scenes::transform>(white_piece);
      white_piece_transform.set_position(sbx::math::vector3{static_cast<float>(i) * 2.0f - 5.0f, 0.7f, -10.0f - static_cast<float>(row) * 2.0f});
      white_piece_transform.set_scale(sbx::math::vector3{8.0f, 8.0f, 8.0f});

      auto black_piece = scene.create_node(fmt::format("Chess_{}_Black_{}", piece_name, row));

      scene.add_component<sbx::scenes::static_mesh>(black_piece, scene.get_mesh("chess_pieces"), std::vector<sbx::scenes::static_mesh::submesh>{{i, scene.get_material("chess_pawn_black")}});

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

  fox1 = scene.create_node("Fox");

  animations_module.add_animated_mesh(fox1, scene.get_mesh("fox"), scene.get_material("fox"));

  auto& fox1_animator = scene.add_component<sbx::animations::animator>(fox1);

  fox1_animator.add_state({"Walk", scene.get_animation("Walk"), true, 1.5f});

  fox1_animator.play("Walk", true);

  auto& fox1_transform = scene.get_component<sbx::scenes::transform>(fox1);
  fox1_transform.set_position(sbx::math::vector3{15.0f, 0.0f, 0.0f});
  fox1_transform.set_scale(sbx::math::vector3{0.06f, 0.06f, 0.06f});

  // Fox2

  fox2 = scene.create_node("Fox");

  animations_module.add_animated_mesh(fox2, scene.get_mesh("fox"), scene.get_material("fox"));

  auto fox_tail_node = animations_module.find_skeleton_node(fox2, "b_Tail03_014");

  if (fox_tail_node != sbx::scenes::node::null) {
    auto test = scene.create_child_node(fox_tail_node, "Test");

    scene.add_component<sbx::scenes::static_mesh>(test, scene.get_mesh("sphere"), scene.get_material("fox"));

    auto& test_transform = scene.get_component<sbx::scenes::transform>(test);
    test_transform.set_scale(sbx::math::vector3{10.0f, 10.0f, 10.0f});
  }

  auto& fox2_animator = scene.add_component<sbx::animations::animator>(fox2);

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

  auto& fox2_transform = scene.get_component<sbx::scenes::transform>(fox2);
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
      material.alpha = sbx::models::alpha_mode::opaque;
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
  auto camera = scene.camera();

  scene.add_component<sbx::scenes::skybox>(camera, scene.get_cube_image("skybox"), _brdf, _irradiance, _prefiltered);

  auto camera_controller_script = scripting_module.instantiate(camera, "build/x86_64/gcc/debug/_dotnet/Demo.dll", "Demo.CameraController");

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

  _rotation += sbx::math::degree{45} * delta_time;

  // scenes_module.add_debug_sphere(sbx::math::vector3{0.0f, 15.0f, 0.0f}, 5.0f, sbx::math::color::red());

  // _camera_controller.update();

  // static auto fox_speed = 0.0f;
  // static auto direction = 1;

  // fox_speed += direction * 0.2f * delta_time;

  // if (fox_speed > 2.5f) {
  //   fox_speed = 2.5f;
  //   direction = -1;
  // } else if (fox_speed < 0.0f) {
  //   fox_speed = 0.0f;
  //   direction = 1;
  // }

  // auto& fox2_animator = scene.get_component<sbx::animations::animator>(fox2);
  // fox2_animator.set_float("speed", fox_speed);

  // auto& fox_transform = scene.get_component<sbx::scenes::transform>(fox2);
  // fox_transform.set_rotation(sbx::math::vector3::up, _rotation);

  if (scene.is_valid(_light_center)) {
    auto& light_center_transform = scene.get_component<sbx::scenes::transform>(_light_center);
    light_center_transform.set_rotation(sbx::math::vector3::up, _rotation);
  }
}

auto application::fixed_update() -> void {

}

#if defined(DUMP_IMAGES) && (DUMP_IMAGES == 1)

static inline auto half_to_float(std::uint16_t h) -> float {
  auto h_exp  = std::uint16_t{(h & 0x7C00u) >> 10};
  auto h_frac = std::uint16_t{h & 0x03FFu};
  auto h_sign = std::uint16_t{h & 0x8000u};

  auto f_sign = std::uint32_t{h_sign} << 16;
  auto f_exp = std::uint32_t{0};
  auto f_frac = std::uint32_t{0};

  if (h_exp == 0) {
    if (h_frac == 0) {
      f_exp = 0;
      f_frac = 0;
    } else {
      h_frac <<= 1;
      h_exp = 1;
      while ((h_frac & 0x0400u) == 0) { h_frac <<= 1; h_exp--; }
      h_frac &= 0x03FFu;
      f_exp = std::uint32_t{h_exp + 112} << 23;
      f_frac = std::uint32_t{h_frac} << 13;
    }
  } else if (h_exp == 0x1F) {
    f_exp = 0xFFu << 23;
    f_frac = std::uint32_t{h_frac} << 13;
  } else {
    f_exp = std::uint32_t{h_exp + 112} << 23;
    f_frac = std::uint32_t{h_frac} << 13;
  }

  auto f = std::uint32_t{f_sign | f_exp | f_frac};

  auto out = std::float_t{};
  std::memcpy(&out, &f, sizeof(std::float_t));

  return out;
}


static inline auto _dump_image2d(const sbx::graphics::image2d& image, const std::filesystem::path& path) -> void {
  auto buffer = sbx::graphics::buffer{image.size().y() * image.size().y() * 4 * sizeof(std::float_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

  sbx::graphics::image::copy_image_to_buffer(image, VK_FORMAT_R16G16_SFLOAT, buffer, {0, 0, 0}, {image.size().x(), image.size().y(), 1}, 0, 1, 0);

  buffer.map();

  auto* src = reinterpret_cast<std::uint16_t*>(buffer.mapped_memory().get());

  auto png = std::vector<std::uint8_t>{};
  png.resize(image.size().x() * image.size().y() * 3);

  for (auto i = 0; i < image.size().x() * image.size().y(); i++) {
    auto a = half_to_float(src[i * 2 + 0]);
    auto b = half_to_float(src[i * 2 + 1]);
    png[i * 3 + 0] = std::uint8_t{std::clamp(a, 0.f, 1.f) * 255.f};
    png[i * 3 + 1] = std::uint8_t{std::clamp(b, 0.f, 1.f) * 255.f};
    png[i * 3 + 2] = 0;
  }

  stbi_write_png(path.string().c_str(), image.size().x(), image.size().y(), 3, png.data(), image.size().x() * 3);

  buffer.unmap();
}

static inline auto _dump_cubemap_to_png(const sbx::graphics::cube_image& cube, const std::filesystem::path& base_name) -> void {
  const auto size = cube.size().x();
  const auto mip_levels = cube.mip_levels();
  const auto format = cube.format();

  auto bpp = std::uint32_t{0};

  if (format == VK_FORMAT_R32G32B32A32_SFLOAT) {
    bpp = sizeof(std::float_t) * 4;
  } else if (format == VK_FORMAT_R16G16B16A16_SFLOAT) {
    bpp = sizeof(std::uint16_t) * 4;
  } else if (format == VK_FORMAT_R8G8B8A8_UNORM) {
    bpp = 4;
  } else { 
    throw std::runtime_error{"Unsupported format for cubemap PNG dump"};
  }

  for (auto mip = 0u; mip < mip_levels; ++mip) {
    const auto mip_width = std::max(1u, size >> mip);
    const auto mip_height = std::max(1u, size >> mip);
    const auto buffer_size = static_cast<size_t>(mip_width) * static_cast<size_t>(mip_height) * bpp;

    for (auto face = 0u; face < 6; ++face) {
      auto staging = sbx::graphics::buffer{buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

      sbx::graphics::image::copy_image_to_buffer(cube.handle(), format, staging.handle(), {0, 0, 0}, {mip_width, mip_height, 1}, mip, 1, face);

      staging.map();

      auto* raw = staging.mapped_memory().get();
      auto png = std::vector<std::uint8_t>(mip_width * mip_height * 3);

      if (format == VK_FORMAT_R32G32B32A32_SFLOAT) {
        auto src = reinterpret_cast<float*>(raw);

        for (auto i = 0u; i < mip_width * mip_height; ++i) {
          png[i * 3 + 0] = std::uint8_t(std::clamp(src[i * 4 + 0], 0.f, 1.f) * 255.f);
          png[i * 3 + 1] = std::uint8_t(std::clamp(src[i * 4 + 1], 0.f, 1.f) * 255.f);
          png[i * 3 + 2] = std::uint8_t(std::clamp(src[i * 4 + 2], 0.f, 1.f) * 255.f);
        }
      } else if (format == VK_FORMAT_R16G16B16A16_SFLOAT) {
        auto src = reinterpret_cast<std::uint16_t*>(raw);

        for (auto i = 0u; i < mip_width * mip_height; ++i) {
          png[i * 3 + 0] = std::uint8_t(std::clamp(half_to_float(src[i * 4 + 0]), 0.f, 1.f) * 255.f);
          png[i * 3 + 1] = std::uint8_t(std::clamp(half_to_float(src[i * 4 + 1]), 0.f, 1.f) * 255.f);
          png[i * 3 + 2] = std::uint8_t(std::clamp(half_to_float(src[i * 4 + 2]), 0.f, 1.f) * 255.f);
        }
      } else if (format == VK_FORMAT_R8G8B8A8_UNORM) {
        auto src = reinterpret_cast<std::uint8_t*>(raw);

        for (auto i = 0u; i < mip_width * mip_height; ++i) {
          png[i * 3 + 0] = src[i * 4 + 0];
          png[i * 3 + 1] = src[i * 4 + 1];
          png[i * 3 + 2] = src[i * 4 + 2];
        }
      }

      staging.unmap();

      auto filename = fmt::format("{}_face{}_mip{}.png", base_name.string(), face, mip);

      stbi_write_png(filename.c_str(), mip_width, mip_height, 3, png.data(), mip_width * 3);
    }
  }
}

#endif // DUMP_IMAGES

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

  sbx::utility::logger<"application">::info("Generated 'brdf' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()).value());

#if defined(DUMP_IMAGES) && (DUMP_IMAGES == 1)
  _dump_image2d(brdf, "brdf.png");
#endif
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

  sbx::utility::logger<"application">::info("Generated 'irradiance' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()).value());

#if defined(DUMP_IMAGES) && (DUMP_IMAGES == 1)
  _dump_cubemap_to_png(irradiance, "irradiance");
#endif
}

// auto application::_generate_prefiltered(uint32_t size) -> void {
//   auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
//   auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
//   auto& scene = scenes_module.scene();

//   _prefiltered = graphics_module.add_resource<sbx::graphics::cube_image>(sbx::math::vector2u{size}, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLE_COUNT_1_BIT, true, true);

//   auto timer = sbx::utility::timer{};

//   auto& prefiltered = graphics_module.get_resource<sbx::graphics::cube_image>(_prefiltered);

//   auto command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_COMPUTE_BIT};

//   auto pipeline = sbx::graphics::compute_pipeline{"res://shaders/prefiltered"};
//   auto descriptor_handler = sbx::graphics::descriptor_handler{pipeline, 0u};
//   auto push_handler = sbx::graphics::push_handler{pipeline};

//   pipeline.bind(command_buffer);

//   auto mip_views = std::vector<VkImageView>{};
//   mip_views.resize(prefiltered.mip_levels());

//   for (auto mip = 0; mip < prefiltered.mip_levels(); ++mip) {
//     sbx::graphics::image::create_image_view(prefiltered, mip_views[mip], VK_IMAGE_VIEW_TYPE_2D_ARRAY, prefiltered.format(), VK_IMAGE_ASPECT_COLOR_BIT, 1, mip, 6, 0);
//   }

//   auto& skybox = graphics_module.get_resource<sbx::graphics::cube_image>(scene.get_cube_image("skybox"));

//   for (auto mip = 0; mip < prefiltered.mip_levels(); ++mip) {
//     auto barrier = VkImageMemoryBarrier2{};
//     barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
//     barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
//     barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
//     barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
//     barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
//     barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
//     barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
//     barrier.image = prefiltered;
//     barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     barrier.subresourceRange.baseMipLevel = mip;
//     barrier.subresourceRange.levelCount = 1;
//     barrier.subresourceRange.baseArrayLayer = 0;
//     barrier.subresourceRange.layerCount = 6;

//     auto dependency = VkDependencyInfo{};
//     dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
//     dependency.imageMemoryBarrierCount = 1;
//     dependency.pImageMemoryBarriers = &barrier;

//     vkCmdPipelineBarrier2(command_buffer, &dependency);

//     auto descriptor_image_info = std::vector<VkDescriptorImageInfo>{};

//     descriptor_image_info.push_back(VkDescriptorImageInfo{
//       .sampler = VK_NULL_HANDLE, // prefiltered.sampler(),
//       .imageView = mip_views[mip],
//       .imageLayout = VK_IMAGE_LAYOUT_GENERAL
//     });

//     auto descriptor_write = VkWriteDescriptorSet{};
//     descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//     descriptor_write.dstBinding = *pipeline.find_descriptor_binding("output", 0u);
//     descriptor_write.dstArrayElement = 0;
//     descriptor_write.descriptorCount = 1;
//     descriptor_write.descriptorType = *pipeline.find_descriptor_type_at_binding(0u, descriptor_write.dstBinding);

//     auto write_set = sbx::graphics::write_descriptor_set{descriptor_write, descriptor_image_info};

//     descriptor_handler.push("skybox", skybox);
//     descriptor_handler.push("output", prefiltered, std::move(write_set));

//     descriptor_handler.update(pipeline);
//     descriptor_handler.bind_descriptors(command_buffer);

//     const auto roughness = static_cast<std::float_t>(mip) / static_cast<std::float_t>(prefiltered.mip_levels() - 1);

//     push_handler.push("roughness", roughness);
//     // push_handler.push("num_samples", 32u);
//     push_handler.bind(command_buffer);

//     const auto group_count_x = static_cast<std::uint32_t>(std::ceil(static_cast<std::float_t>(prefiltered.size().x() >> mip) / static_cast<std::float_t>(16)));
//     const auto group_count_y = static_cast<std::uint32_t>(std::ceil(static_cast<std::float_t>(prefiltered.size().y() >> mip) / static_cast<std::float_t>(16)));

//     pipeline.dispatch(command_buffer, {group_count_x, group_count_y, 1});
//   }

//   auto barrier = VkImageMemoryBarrier2{};
//   barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
//   barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
//   barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
//   barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
//   barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
//   barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
//   barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//   barrier.image = prefiltered;
//   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//   barrier.subresourceRange.baseMipLevel = 0;
//   barrier.subresourceRange.levelCount = prefiltered.mip_levels();
//   barrier.subresourceRange.baseArrayLayer = 0;
//   barrier.subresourceRange.layerCount = 6;

//   auto dependency = VkDependencyInfo{};
//   dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
//   dependency.imageMemoryBarrierCount = 1;
//   dependency.pImageMemoryBarriers = &barrier;

//   vkCmdPipelineBarrier2(command_buffer, &dependency);

//   command_buffer.submit_idle();

//   for (auto& view : mip_views) {
//     vkDestroyImageView(graphics_module.logical_device(), view, nullptr);
//   }

//   sbx::utility::logger<"application">::info("Generated 'prefiltered' with {} mips in {:.2f}ms", prefiltered.mip_levels(), sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()).value());

// #if defined(DUMP_IMAGES) && (DUMP_IMAGES == 1)
//   _dump_cubemap_to_png(prefiltered, "prefiltered");
// #endif
// }

// [TODO] KAJ 2025-11-27 : Figure out how to fix descriptor handler for this use case
auto application::_generate_prefiltered(uint32_t size) -> void {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module   = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene           = scenes_module.scene();

  // 1. Create prefiltered cube image (use rgba32f to match RWTexture2DArray<float4>)
  _prefiltered = graphics_module.add_resource<sbx::graphics::cube_image>(
    sbx::math::vector2u{size},
    VK_FORMAT_R32G32B32A32_SFLOAT,
    VK_IMAGE_LAYOUT_GENERAL,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
    VK_IMAGE_USAGE_STORAGE_BIT |
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    VK_FILTER_LINEAR,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    VK_SAMPLE_COUNT_1_BIT,
    true,  // anisotropic
    true   // mipmap
  );

  auto timer = sbx::utility::timer{};

  auto& prefiltered = graphics_module.get_resource<sbx::graphics::cube_image>(_prefiltered);
  auto& skybox = graphics_module.get_resource<sbx::graphics::cube_image>(scene.get_cube_image("skybox"));

  const uint32_t mip_levels = prefiltered.mip_levels();

  // 2. Command buffer for compute
  auto command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_COMPUTE_BIT};

  sbx::graphics::compute_pipeline pipeline{ "res://shaders/prefiltered" };
  pipeline.bind(command_buffer);

  // Optional: push handler if you want to use reflection-based push constants
  sbx::graphics::push_handler push_handler{ pipeline };

  // 3. Create per-mip 2D array views for the storage image
  std::vector<VkImageView> mip_views(mip_levels);

  for (uint32_t mip = 0; mip < mip_levels; ++mip) {
    sbx::graphics::image::create_image_view(
      prefiltered,
      mip_views[mip],
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,            // storage image view
      prefiltered.format(),
      VK_IMAGE_ASPECT_COLOR_BIT,
      1,                                      // levelCount
      mip,                                    // baseMipLevel
      6,                                      // layerCount (6 faces)
      0                                       // baseArrayLayer
    );
  }

  // 4. Prepare manual descriptor wiring
  VkDevice device = graphics_module.logical_device();

  // You may need to adapt these accessors to your pipeline implementation
  VkPipelineLayout       pipeline_layout = pipeline.layout();
  VkDescriptorSetLayout  set_layout      = pipeline.descriptor_set_layout(0);
  VkDescriptorPool       descriptor_pool = pipeline.descriptor_pool();

  // 5. For each mip level, allocate a descriptor set and dispatch
  for (uint32_t mip = 0; mip < mip_levels; ++mip) {
    const uint32_t mip_width  = std::max(1u, prefiltered.size().x() >> mip);
    const uint32_t mip_height = std::max(1u, prefiltered.size().y() >> mip);

    // (Optional) barrier: ensure we're in GENERAL for this mip
    {
      VkImageMemoryBarrier2 barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      barrier.srcStageMask  = VK_PIPELINE_STAGE_2_NONE;
      barrier.srcAccessMask = 0;
      barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
      barrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
      barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
      barrier.image         = static_cast<VkImage>(prefiltered);
      barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.subresourceRange.baseMipLevel   = mip;
      barrier.subresourceRange.levelCount     = 1;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount     = 6;

      VkDependencyInfo dep{};
      dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      dep.imageMemoryBarrierCount = 1;
      dep.pImageMemoryBarriers    = &barrier;

      vkCmdPipelineBarrier2(command_buffer, &dep);
    }

    // 5.1 Allocate descriptor set for this mip
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool     = descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts        = &set_layout;

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set) != VK_SUCCESS) {
      throw std::runtime_error{ "Failed to allocate descriptor set for prefiltered env" };
    }

    // 5.2 Setup descriptor image infos

    // Binding 0: skybox samplerCube
    VkDescriptorImageInfo skybox_info{};
    skybox_info.sampler     = skybox.sampler();     // combined image sampler
    skybox_info.imageView   = skybox.view();        // cube view
    skybox_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Binding 1: storage image (RWTexture2DArray<float4>)
    VkDescriptorImageInfo prefiltered_info{};
    prefiltered_info.sampler     = VK_NULL_HANDLE;  // storage image -> no sampler
    prefiltered_info.imageView   = mip_views[mip];  // 2D_ARRAY view for this mip
    prefiltered_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2]{};

    // Skybox
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = descriptor_set;
    writes[0].dstBinding      = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo      = &skybox_info;

    // Prefiltered storage image
    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = descriptor_set;
    writes[1].dstBinding      = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo      = &prefiltered_info;

    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

    // 5.3 Bind descriptor set
    vkCmdBindDescriptorSets(
      command_buffer,
      VK_PIPELINE_BIND_POINT_COMPUTE,
      pipeline_layout,
      0,                        // firstSet
      1, &descriptor_set,
      0, nullptr
    );

    // 5.4 Push constants (roughness + numSamples)
    const float roughness = static_cast<float>(mip) / static_cast<float>(std::max(1u, mip_levels - 1));

    // If your shader uses:
    // struct push_data { float roughness; uint numSamples; };
    push_handler.push("roughness",  roughness);
    // push_handler.push("num_samples", 32u); // or whatever you like
    push_handler.bind(command_buffer);

    // 5.5 Dispatch compute for this mip
    const uint32_t group_count_x = (mip_width + threads_per_group - 1) / threads_per_group;
    const uint32_t group_count_y = (mip_height + threads_per_group - 1) / threads_per_group;

    pipeline.dispatch(command_buffer, { group_count_x, group_count_y, 1u });

    // You can optionally free descriptor_set here if your pool is transient,
    // or let it be destroyed with the pool.
  }

  // 6. Transition the whole image to SHADER_READ_ONLY_OPTIMAL for sampling in PBR shader
  {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.image         = static_cast<VkImage>(prefiltered);
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 6;

    VkDependencyInfo dep{};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers    = &barrier;

    vkCmdPipelineBarrier2(command_buffer, &dep);
  }

  // 7. Submit and wait
  command_buffer.submit_idle();

  // 8. Destroy temporary per-mip views
  for (auto& view : mip_views) {
    vkDestroyImageView(graphics_module.logical_device(), view, nullptr);
  }

  sbx::utility::logger<"application">::info(
    "Generated 'prefiltered' with {} mips in {:.2f}ms",
    mip_levels,
    sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()).value()
  );

  // 9. Debug dump to PNG
#if defined(DUMP_IMAGES) && (DUMP_IMAGES == 1)
  _dump_cubemap_to_png(prefiltered, "prefiltered");
#endif
}


} // namespace demo
