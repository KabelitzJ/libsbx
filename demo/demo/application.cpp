// SPDX-License-Identifier: MIT
#include <demo/application.hpp>

#include <nlohmann/json.hpp>

#include <easy/profiler.h>

#include <libsbx/math/color.hpp>
#include <libsbx/math/noise.hpp>
#include <libsbx/math/constants.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <demo/renderer.hpp>

#include <libsbx/devices/input.hpp>

#include <libsbx/scenes/debug_subrenderer.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/skinned_mesh.hpp>

#include <libsbx/scripting/scripting.hpp>

#include <libsbx/animations/mesh.hpp>
#include <libsbx/animations/animation.hpp>
#include <libsbx/animations/animator.hpp>
#include <libsbx/animations/animations_module.hpp>

#include <libsbx/sprites/sprite_subrenderer.hpp>

#include <libsbx/particles/particle_emitter.hpp>

#include <libsbx/physics/mesh_collider.hpp>
#include <libsbx/physics/shape_collider.hpp>
#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/physics_module.hpp>

#include <libsbx/audio/audio_module.hpp>

namespace demo {

application::application()
: sbx::core::application{},
  _is_paused{false},
  _rotation{sbx::math::degree{0}} { 
  // Renderer
  const auto& cli = sbx::core::engine::cli();

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (auto assets = cli.argument<std::string>("assets"); assets) {
    assets_module.set_asset_root(*assets);
  } else {
    assets_module.set_asset_root("demo/assets");
  }

  auto& audio_module = sbx::core::engine::get_module<sbx::audio::audio_module>();

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  graphics_module.set_renderer<demo::renderer>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.load_scene("res://scenes/scene.yaml");

  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  // Textures
  scene.add_image("base", "res://textures/base.png", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("fire", "res://textures/fire/fire.png", sbx::graphics::format::r8g8b8a8_srgb);

  scene.add_image("rune0", "res://textures/runes/rune0.png", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("rune1", "res://textures/runes/rune1.png", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("rune2", "res://textures/runes/rune2.png", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("rune3", "res://textures/runes/rune3.png", sbx::graphics::format::r8g8b8a8_srgb);

  scene.add_image("fox_albedo", "res://textures/fox/albedo.png", sbx::graphics::format::r8g8b8a8_srgb);

  scene.add_image("helmet_albedo", "res://meshes/helmet/textures/albedo.jpg", sbx::graphics::format::r8g8b8a8_srgb);
  scene.add_image("helmet_normal", "res://meshes/helmet/textures/normal.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  scene.add_image("helmet_mr", "res://meshes/helmet/textures/mr.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  scene.add_image("helmet_ao", "res://meshes/helmet/textures/ao.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  scene.add_image("helmet_emissive", "res://meshes/helmet/textures/emissive.jpg", sbx::graphics::format::r8g8b8a8_srgb);

  // scene.add_cube_image("skybox", "res://skyboxes/clouds3", ".hdr", sbx::graphics::format::r32g32b32a32_sfloat);
  scene.add_cube_image("skybox", "res://skyboxes/clouds2", ".png", sbx::graphics::format::r8g8b8a8_srgb);

  _generate_brdf(512);
  _generate_irradiance(64);
  _generate_prefiltered(512);

  // Meshes
  scene.add_mesh<sbx::models::mesh>("edge_one", "res://meshes/terrain/edge_one/edge_one.gltf");
  scene.add_mesh<sbx::models::mesh>("edge_three", "res://meshes/terrain/edge_three/edge_three.gltf");
  scene.add_mesh<sbx::models::mesh>("diagonal", "res://meshes/terrain/diagonal/diagonal.gltf");
  scene.add_mesh<sbx::models::mesh>("full", "res://meshes/terrain/full/full.gltf");
  scene.add_mesh<sbx::models::mesh>("half", "res://meshes/terrain/half/half.gltf");

  scene.add_mesh<sbx::models::mesh>("cube", "res://meshes/cube/cube.gltf");

  scene.add_mesh<sbx::models::mesh>("sponza", "res://meshes/sponza/sponza.gltf");

  scene.add_mesh<sbx::models::mesh>("helmet", "res://meshes/helmet/helmet.gltf");

  scene.add_mesh<sbx::animations::mesh>("fox", "res://meshes/fox/fox.gltf");

  // Materials

  auto& base_material = scene.add_material<sbx::models::material>("base");
  base_material.albedo.image = scene.get_image("base");

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

  // Sponza

  // auto sponza = scene.create_node("Sponza");

  // scene.add_component<sbx::scenes::static_mesh>(sponza, scene.get_mesh("sponza"), sbx::models::load_materials("res://meshes/sponza/sponza.gltf"));

  // auto& sponza_transform = scene.get_component<sbx::scenes::transform>(sponza);
  // sponza_transform.set_scale(sbx::math::vector3{1.0f, 1.0f, 1.0f});

  // Helmet

  auto helmet = scene.create_node("Helmet", sbx::scenes::transform{});

  scene.add_component<sbx::scenes::static_mesh>(helmet, scene.get_mesh("helmet"), sbx::models::load_materials("res://meshes/helmet/helmet.gltf"));

  auto& helmet_transform = scene.get_component<sbx::scenes::transform>(helmet);
  helmet_transform.set_position(sbx::math::vector3{0.0f, 3.0f, 0.0f});
  helmet_transform.set_scale(sbx::math::vector3{1.0f, 1.0f, 1.0f});

  auto& helmet_collider = scene.add_component<sbx::physics::mesh_collider>(helmet, "res://meshes/helmet/helmet.gltf");

  // Terrain

  auto terrain = scene.create_node("Terrain");

  auto& terrain_material = scene.add_material<sbx::models::material>("terrain");
  terrain_material.albedo.image = scene.get_image("base");

  scene.add_component<sbx::scenes::static_mesh>(terrain, scene.get_mesh("cube"), scene.get_material("terrain"));

  auto& transform = scene.get_component<sbx::scenes::transform>(terrain);
  transform.set_position(sbx::math::vector3{0.0f, -1.5f, 0.0f});
  transform.set_scale(sbx::math::vector3{400.0f, 0.5f, 400.0f});

  scene.add_component<sbx::physics::shape_collider>(terrain, sbx::physics::box{sbx::math::vector3{200.0f, 0.25f, 200.0f}});
  scene.add_component<sbx::physics::rigidbody>(terrain, 0.0f);

  // _rune0_emitter = scene.create_node("Rune0Emitter");

  // auto& rune0_emitter = scene.add_component<sbx::particles::particle_emitter>(_rune0_emitter);
  // rune0_emitter.max_particles = 2000;
  // rune0_emitter.emission_rate = 50.0f;
  // rune0_emitter.emission_shape = sbx::math::volume{{-2.3f, 0.0f, -2.3f}, {2.3f, 0.0f, 2.3f}};
  // rune0_emitter.initial_speed = sbx::math::vector2{0.5f, 1.0f};
  // rune0_emitter.initial_lifetime = sbx::math::vector2{1.5f, 3.2f};
  // rune0_emitter.initial_size = sbx::math::vector2{0.3f, 0.6f};
  // rune0_emitter.initial_rotation = sbx::math::vector2{0.0f, 0.0f};
  // rune0_emitter.initial_color = sbx::math::color{101u, 213u, 253u, 250u};
  // rune0_emitter.gravity = sbx::math::vector3{0.0f, 3.0f, 0.0f};
  // rune0_emitter.drag = 1.0f;
  // rune0_emitter.end_color = sbx::math::color{8u, 145u, 195u, 0u};
  // rune0_emitter.end_size_scale = 0.2f;
  // rune0_emitter.images = {
  //   scene.get_image("rune0"),
  //   scene.get_image("rune1"),
  //   scene.get_image("rune2"),
  //   scene.get_image("rune3")
  // };

  // auto& rune0_emitter_transform = scene.get_component<sbx::scenes::transform>(_rune0_emitter);
  // rune0_emitter_transform.set_position(sbx::math::vector3{0.0f, 15.0f, 0.0f});

  // Fox
  auto& animations_module = sbx::core::engine::get_module<sbx::animations::animations_module>();

  auto fox1 = scene.create_node("Fox");

  auto& fox_material = scene.add_material<sbx::models::material>("fox");
  fox_material.albedo.image = scene.get_image("fox_albedo");
  fox_material.metallic_factor = 0.0f;
  fox_material.roughness_factor = 1.0f;

  animations_module.add_animated_mesh(fox1, scene.get_mesh("fox"), scene.get_material("fox"));

  auto tail = animations_module.find_skeleton_node(fox1, "b_Tail03_014");

  if (tail != sbx::scenes::node::null) {
    auto tail_emitter = scene.create_child_node(tail, "TailEmitter");

    auto& tail_particle_emitter = scene.add_component<sbx::particles::particle_emitter>(tail_emitter);
    tail_particle_emitter.max_particles = 1000;
    tail_particle_emitter.emission_rate = 100.0f;
    tail_particle_emitter.emission_shape = sbx::math::volume{{-0.1f, 0.0f, -0.1f}, {0.1f, 0.0f, 0.1f}};
    tail_particle_emitter.initial_speed = sbx::math::vector2{1.0f, 2.0f};
    tail_particle_emitter.initial_lifetime = sbx::math::vector2{0.5f, 1.0f};
    tail_particle_emitter.initial_size = sbx::math::vector2{0.2f, 0.4f};
    tail_particle_emitter.initial_rotation = sbx::math::vector2{0.0f, 0.0f};
    tail_particle_emitter.initial_color = sbx::math::color{255u, 140u, 0u, 250u};
    tail_particle_emitter.gravity = sbx::math::vector3{0.0f, 1.0f, 0.0f};
    tail_particle_emitter.drag = 0.5f;
    tail_particle_emitter.end_color = sbx::math::color{255u, 69u, 0u, 0u};
    tail_particle_emitter.end_size_scale = 0.1f;
    tail_particle_emitter.images = {
      scene.get_image("fire")
    };

    auto & tail_emitter_transform = scene.get_component<sbx::scenes::transform>(tail_emitter);
    tail_emitter_transform.set_position(sbx::math::vector3{0.0f, 0.0f, 0.0f});
  }

  auto& fox_animator = scene.add_component<sbx::animations::animator>(fox1);

  fox_animator.add_state({"Walk", scene.get_animation("Walk"), true, 0.5f });
  fox_animator.add_state({"Survey", scene.get_animation("Survey"), true, 0.5f });
  fox_animator.add_state({"Run", scene.get_animation("Run"), true, 0.5f });

  fox_animator.add_transition({
    "Walk", "Survey", 0.20f,
    [](const sbx::animations::animator& animator){
      if (auto value = animator.float_parameter("speed"); value) {
        return *value <= 0.05f;
      }

      return false;
    }
  });

  fox_animator.add_transition({
    "Run", "Survey", 0.25f,
    [](const sbx::animations::animator& animator){
      if (auto value = animator.float_parameter("speed"); value) {
        return *value <= 0.05f;
      }

      return false;
    }
  });

  // Walk ↔ Run thresholds
  fox_animator.add_transition({
    "Walk", "Run", 0.15f,
    [](const sbx::animations::animator& animator){
      if (auto value = animator.float_parameter("speed"); value) {
        return *value >= 2.0f;
      }

      return false;
    }
  });

  fox_animator.add_transition({
    "Run", "Walk", 0.15f,
    [](const sbx::animations::animator& animator){
      if (auto value = animator.float_parameter("speed"); value) {
        return *value < 2.0f && *value > 0.05f;
      }

      return false;
    }
  });

  // Survey → Walk when starting to move
  fox_animator.add_transition({
    "Survey", "Walk", 0.20f,
    [](const sbx::animations::animator& animator){
      if (auto value = animator.float_parameter("speed"); value) {
        return *value > 0.05f && *value < 2.0f;
      }

      return false;
    }
  });

  fox_animator.play("Survey", true);
  fox_animator.set_float("speed", 1.0f);

  auto& fox1_transform = scene.get_component<sbx::scenes::transform>(fox1);
  fox1_transform.set_position(sbx::math::vector3{0.0f, 0.0f, 18.0f});
  fox1_transform.set_scale(sbx::math::vector3{0.06f, 0.06f, 0.06f});

  // Camera
  auto camera_node = scene.camera();

  auto& camera_transform = scene.get_component<sbx::scenes::transform>(camera_node);
  camera_transform.set_position(sbx::math::vector3{0.0f, 10.0f, 20.0f});
  camera_transform.look_at(sbx::math::vector3::zero);

  scene.add_component<sbx::scenes::skybox>(camera_node, scene.get_cube_image("skybox"), _brdf, _irradiance, _prefiltered, sbx::math::color::white());

  scripting_module.instantiate(camera_node, "build/x86_64/gcc/debug/_dotnet/Demo.dll", "Demo.EditorCameraController");

  if (auto hide_window = cli.argument<bool>("hide-window"); !hide_window) {
    window.show();
  }

  sbx::utility::logger<"demo">::info("string id: {}", sbx::utility::string_id<"foobar">());
}

auto application::update() -> void  {
  SBX_PROFILE_SCOPE("application::update");

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.scene();

  const auto delta_time = sbx::core::engine::delta_time();

  if (sbx::devices::input::is_key_pressed(sbx::devices::key::escape)) {
    sbx::core::engine::quit();
    return;
  }

  _rotation += sbx::math::degree{45} * delta_time;

  // if (sbx::devices::input::is_key_pressed(sbx::devices::key::space)) {
  //   // _is_paused = !_is_paused;

  //   auto cube = scene.create_node("Cube");

  //   auto& terrain_material = scene.add_material<sbx::models::material>("cube");
  //   terrain_material.albedo.image = scene.get_image("base");

  //   scene.add_component<sbx::scenes::static_mesh>(cube, scene.get_mesh("cube"), scene.get_material("cube"));

  //   auto& transform = scene.get_component<sbx::scenes::transform>(cube);
  //   transform.set_position(sbx::math::vector3{-6.0f, 12.0f, 0.0f});
  //   transform.set_rotation(sbx::math::vector3::right, sbx::math::degree{35});

  //   auto& collider = scene.add_component<sbx::physics::shape_collider>(cube, sbx::physics::box{sbx::math::vector3{0.5f, 0.5f, 0.5f}});

  //   auto& rigidbody = scene.add_component<sbx::physics::rigidbody>(cube, 1.0f);
  //   rigidbody.add_constant_acceleration(sbx::math::vector3{0.0f, -9.81f, 0.0f});
  //   rigidbody.set_inverse_inertia_tensor(sbx::physics::inverse_inertia_tensor(collider, rigidbody.mass()));
  // }

  // if (sbx::devices::input::is_key_pressed(sbx::devices::key::j)) {
  //   auto& fire_emitter = scene.get_component<sbx::particles::particle_emitter>(_rune0_emitter);

  //   if (fire_emitter.is_playing()) {
  //     fire_emitter.pause();
  //   } else {
  //     fire_emitter.play();
  //   }
  // }
}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

auto application::_generate_brdf(const std::uint32_t size) -> void {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  const auto& logical_device = graphics_module.logical_device();

  const auto& graphics_queue = logical_device.queue<sbx::graphics::queue::type::graphics>();
  const auto& compute_queue = logical_device.queue<sbx::graphics::queue::type::compute>();

  _brdf = graphics_module.add_resource<sbx::graphics::image2d>(sbx::math::vector2u{size}, sbx::graphics::format::r16g16_sfloat, sbx::graphics::filter::linear, sbx::graphics::address_mode::repeat);

  auto timer = sbx::utility::timer{};

  auto& brdf = graphics_module.get_resource<sbx::graphics::image2d>(_brdf);

  auto initial_command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_GRAPHICS_BIT};

  sbx::graphics::image::transition_image_layout(initial_command_buffer, brdf.handle(), VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, brdf.mip_levels(), 0, brdf.array_layers(), 0);

  if (graphics_queue.family() != compute_queue.family()) {
    auto brdf_release = sbx::graphics::command_buffer::image_release_data{
      .image = brdf.handle(),
      .mip_levels = brdf.mip_levels(),
      .base_array_layer = 0,
      .layer_count = brdf.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    initial_command_buffer.release_image_ownership({brdf_release});
  }

  initial_command_buffer.submit_idle();

  auto compute_command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_COMPUTE_BIT};

  if (graphics_queue.family() != compute_queue.family()) {
    auto brdf_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = brdf.handle(),
      .mip_levels = brdf.mip_levels(),
      .base_array_layer = 0,
      .layer_count = brdf.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    compute_command_buffer.acquire_image_ownership({brdf_acquire});
  }

  auto pipeline = sbx::graphics::compute_pipeline{"res://shaders/ibl/brdf"};

  pipeline.bind(compute_command_buffer);

  auto descriptor_handler = sbx::graphics::descriptor_handler{pipeline, 0u};

  descriptor_handler.push("output", brdf);

  descriptor_handler.update(pipeline);
  descriptor_handler.bind_descriptors(compute_command_buffer);

  const auto group_count_x = (brdf.size().x() + threads_per_group - 1) / threads_per_group;
  const auto group_count_y = (brdf.size().y() + threads_per_group - 1) / threads_per_group;

  pipeline.dispatch(compute_command_buffer, sbx::math::vector3u{group_count_x, group_count_y, 1u});

  if (graphics_queue.family() != compute_queue.family()) {
    auto brdf_release = sbx::graphics::command_buffer::image_release_data{
      .image = brdf.handle(),
      .mip_levels = brdf.mip_levels(),
      .base_array_layer = 0,
      .layer_count = brdf.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    compute_command_buffer.release_image_ownership({brdf_release});
  } else {
    sbx::graphics::image::transition_image_layout(compute_command_buffer, brdf.handle(), VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, brdf.mip_levels(), 0, brdf.array_layers(), 0);
  }

  compute_command_buffer.submit_idle();

  if (graphics_queue.family() != compute_queue.family()) {
    auto final_command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_GRAPHICS_BIT};

    auto brdf_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = brdf.handle(),
      .mip_levels = brdf.mip_levels(),
      .base_array_layer = 0,
      .layer_count = brdf.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    final_command_buffer.acquire_image_ownership({brdf_acquire});

    final_command_buffer.submit_idle();
  }

  sbx::utility::logger<"application">::info("Generated 'brdf' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
}

auto application::_generate_irradiance(const std::uint32_t size) -> void {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  const auto& logical_device = graphics_module.logical_device();
  const auto& graphics_queue = logical_device.queue<sbx::graphics::queue::type::graphics>();
  const auto& compute_queue = logical_device.queue<sbx::graphics::queue::type::compute>();

  _irradiance = graphics_module.add_resource<sbx::graphics::cube_image>(sbx::math::vector2u{size}, VK_FORMAT_R32G32B32A32_SFLOAT);

  auto timer = sbx::utility::timer{};

  auto& irradiance = graphics_module.get_resource<sbx::graphics::cube_image>(_irradiance);
  auto& skybox = graphics_module.get_resource<sbx::graphics::cube_image>(scene.get_cube_image("skybox"));

  auto initial_command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_GRAPHICS_BIT};

  sbx::graphics::image::transition_image_layout(initial_command_buffer, irradiance.handle(), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, irradiance.mip_levels(), 0, irradiance.array_layers(), 0);

  if (graphics_queue.family() != compute_queue.family()) {
    auto irradiance_release = sbx::graphics::command_buffer::image_release_data{
      .image = irradiance.handle(),
      .mip_levels = irradiance.mip_levels(),
      .base_array_layer = 0,
      .layer_count = irradiance.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    auto skybox_release = sbx::graphics::command_buffer::image_release_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    initial_command_buffer.release_image_ownership({irradiance_release, skybox_release});
  }

  initial_command_buffer.submit_idle();

  auto compute_command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_COMPUTE_BIT};

  if (graphics_queue.family() != compute_queue.family()) {
    auto irradiance_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = irradiance.handle(),
      .mip_levels = irradiance.mip_levels(),
      .base_array_layer = 0,
      .layer_count = irradiance.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    auto skybox_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    compute_command_buffer.acquire_image_ownership({irradiance_acquire, skybox_acquire});
  }

  auto pipeline = sbx::graphics::compute_pipeline{"res://shaders/ibl/irradiance"};

  pipeline.bind(compute_command_buffer);

  auto descriptor_handler = sbx::graphics::descriptor_handler{pipeline, 0u};

  descriptor_handler.push("skybox", skybox);
  descriptor_handler.push("output", irradiance);

  descriptor_handler.update(pipeline);
  descriptor_handler.bind_descriptors(compute_command_buffer);

  const auto group_count_x = (irradiance.size().x() + threads_per_group - 1) / threads_per_group;
  const auto group_count_y = (irradiance.size().y() + threads_per_group - 1) / threads_per_group;

  pipeline.dispatch(compute_command_buffer, sbx::math::vector3u{group_count_x, group_count_y, 1u});

  if (graphics_queue.family() != compute_queue.family()) {
    auto irradiance_release = sbx::graphics::command_buffer::image_release_data{
      .image = irradiance.handle(),
      .mip_levels = irradiance.mip_levels(),
      .base_array_layer = 0,
      .layer_count = irradiance.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    auto skybox_release = sbx::graphics::command_buffer::image_release_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    compute_command_buffer.release_image_ownership({irradiance_release, skybox_release});
  } else {
    sbx::graphics::image::transition_image_layout(compute_command_buffer, irradiance.handle(), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, irradiance.mip_levels(), 0, irradiance.array_layers(), 0);
  }

  compute_command_buffer.submit_idle();

  if (graphics_queue.family() != compute_queue.family()) {
    auto final_command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_GRAPHICS_BIT};

    auto irradiance_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = irradiance.handle(),
      .mip_levels = irradiance.mip_levels(),
      .base_array_layer = 0,
      .layer_count = irradiance.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    auto skybox_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    final_command_buffer.acquire_image_ownership({irradiance_acquire, skybox_acquire});

    final_command_buffer.submit_idle();
  }

  sbx::utility::logger<"application">::info("Generated 'irradiance' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
}

auto application::_generate_prefiltered(uint32_t size) -> void {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  const auto& logical_device = graphics_module.logical_device();
  const auto& graphics_queue = logical_device.queue<sbx::graphics::queue::type::graphics>();
  const auto& compute_queue = logical_device.queue<sbx::graphics::queue::type::compute>();

  _prefiltered = graphics_module.add_resource<sbx::graphics::cube_image>(sbx::math::vector2u{size}, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLE_COUNT_1_BIT, true, true);

  auto timer = sbx::utility::timer{};

  auto& prefiltered = graphics_module.get_resource<sbx::graphics::cube_image>(_prefiltered);
  auto& skybox = graphics_module.get_resource<sbx::graphics::cube_image>(scene.get_cube_image("skybox"));

  auto initial_command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_GRAPHICS_BIT};

  sbx::graphics::image::transition_image_layout(initial_command_buffer, prefiltered.handle(), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, prefiltered.mip_levels(), 0, prefiltered.array_layers(), 0);

  if (graphics_queue.family() != compute_queue.family()) {
    auto prefiltered_release = sbx::graphics::command_buffer::image_release_data{
      .image = prefiltered.handle(),
      .mip_levels = prefiltered.mip_levels(),
      .base_array_layer = 0,
      .layer_count = prefiltered.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    auto skybox_release = sbx::graphics::command_buffer::image_release_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    initial_command_buffer.release_image_ownership({prefiltered_release, skybox_release});
  }

  initial_command_buffer.submit_idle();

  auto compute_command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_COMPUTE_BIT};

  if (graphics_queue.family() != compute_queue.family()) {
    auto prefiltered_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = prefiltered.handle(),
      .mip_levels = prefiltered.mip_levels(),
      .base_array_layer = 0,
      .layer_count = prefiltered.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    auto skybox_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    compute_command_buffer.acquire_image_ownership({prefiltered_acquire, skybox_acquire});
  }

  auto pipeline = sbx::graphics::compute_pipeline{"res://shaders/ibl/prefiltered"};
  auto push_handler = sbx::graphics::push_handler{pipeline};

  pipeline.bind(compute_command_buffer);

  auto descriptor_handlers = std::vector<sbx::graphics::descriptor_handler>{};
  descriptor_handlers.reserve(prefiltered.mip_levels());

  for (auto mip = 0u; mip < prefiltered.mip_levels(); ++mip) {
    descriptor_handlers.emplace_back(pipeline, 0u);
  }

  auto mip_views = std::vector<VkImageView>{};
  mip_views.resize(prefiltered.mip_levels());

  for (auto mip = 0u; mip < prefiltered.mip_levels(); ++mip) {
    sbx::graphics::image::create_image_view(prefiltered, mip_views[mip], VK_IMAGE_VIEW_TYPE_2D_ARRAY, prefiltered.format(), VK_IMAGE_ASPECT_COLOR_BIT, 1, mip, 6, 0);
  }

  for (auto mip = 0u; mip < prefiltered.mip_levels(); ++mip) {
    auto barrier = VkImageMemoryBarrier2{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
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

    vkCmdPipelineBarrier2(compute_command_buffer, &dependency);

    auto& descriptor_handler = descriptor_handlers[mip];

    auto descriptor_image_info = std::vector<VkDescriptorImageInfo>{};
    descriptor_image_info.reserve(1u);

    descriptor_image_info.push_back(VkDescriptorImageInfo{
      .sampler = VK_NULL_HANDLE,
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
    descriptor_handler.bind_descriptors(compute_command_buffer);

    const auto roughness = static_cast<std::float_t>(mip) / static_cast<std::float_t>(prefiltered.mip_levels() - 1);

    push_handler.push("roughness", roughness);
    push_handler.bind(compute_command_buffer);

    const auto group_count_x = ((prefiltered.size().x() >> mip) + threads_per_group - 1) / threads_per_group;
    const auto group_count_y = ((prefiltered.size().y() >> mip) + threads_per_group - 1) / threads_per_group;

    pipeline.dispatch(compute_command_buffer, {group_count_x, group_count_y, 1});
  }

  if (graphics_queue.family() != compute_queue.family()) {
    auto prefiltered_release = sbx::graphics::command_buffer::image_release_data{
      .image = prefiltered.handle(),
      .mip_levels = prefiltered.mip_levels(),
      .base_array_layer = 0,
      .layer_count = prefiltered.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    auto skybox_release = sbx::graphics::command_buffer::image_release_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    compute_command_buffer.release_image_ownership({prefiltered_release, skybox_release});
  } else {
    sbx::graphics::image::transition_image_layout(compute_command_buffer, prefiltered.handle(), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, prefiltered.mip_levels(), 0, prefiltered.array_layers(), 0);
  }

  compute_command_buffer.submit_idle();

  if (graphics_queue.family() != compute_queue.family()) {
    auto final_command_buffer = sbx::graphics::command_buffer{true, VK_QUEUE_GRAPHICS_BIT};

    auto prefiltered_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = prefiltered.handle(),
      .mip_levels = prefiltered.mip_levels(),
      .base_array_layer = 0,
      .layer_count = prefiltered.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    auto skybox_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    final_command_buffer.acquire_image_ownership({prefiltered_acquire, skybox_acquire});

    final_command_buffer.submit_idle();
  }

  for (auto& view : mip_views) {
    vkDestroyImageView(graphics_module.logical_device(), view, nullptr);
  }

  sbx::utility::logger<"application">::info("Generated 'prefiltered' with {} mips in {:.2f}ms", prefiltered.mip_levels(), sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
}

} // namespace demo
