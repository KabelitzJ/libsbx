// SPDX-License-Identifier: MIT
#include <editor/application.hpp>

#include <nlohmann/json.hpp>

#include <libsbx/utility/profiler.hpp>

#include <imgui.h>

#include <libsbx/reflection/reflection.hpp>

#include <libsbx/math/color.hpp>
#include <libsbx/math/noise.hpp>
#include <libsbx/math/constants.hpp>

#include <libsbx/assets/assets_module.hpp>

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

#include <libsbx/physics/physics_module.hpp>
#include <libsbx/physics/mesh_collider.hpp>
#include <libsbx/physics/shape_collider.hpp>
#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/physics_module.hpp>

#include <libsbx/ui/ui_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/audio/audio_module.hpp>
#include <libsbx/sprites/sprites_module.hpp>
#include <libsbx/ui/ui_module.hpp>
#include <libsbx/filesystem/filesystem_module.hpp>

#include <editor/renderer.hpp>

namespace editor {

application::application()
: sbx::core::application{},
  _is_paused{false} { 
  // Renderer
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  assets_module.set_asset_root("editor/assets");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  graphics_module.set_renderer<editor::renderer>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.create_scene("Scenee");

  scenes_module.set_scene_viewport("scene");

  auto& graph = scene.graph();
  auto& environment = scene.environment();

  auto& asset_registry = scenes_module.asset_registry();

  asset_registry.request_mesh<sbx::models::mesh>("cube", "res://meshes/cube/cube.gltf");

  asset_registry.request_image("base_albedo", "res://textures/base/albedo.png", sbx::graphics::format::r8g8b8a8_srgb);

  asset_registry.request_image("floor_albedo", "res://textures/floor/albedo.png", sbx::graphics::format::r8g8b8a8_srgb);
  asset_registry.request_image("floor_ao", "res://textures/floor/ao.png", sbx::graphics::format::r8g8b8a8_unorm);
  asset_registry.request_image("floor_arm", "res://textures/floor/arm.png", sbx::graphics::format::r8g8b8a8_unorm);
  asset_registry.request_image("floor_normal", "res://textures/floor/normal.png", sbx::graphics::format::r8g8b8a8_unorm);
  asset_registry.request_image("floor_height", "res://textures/floor/height.png", sbx::graphics::format::r8g8b8a8_unorm);

  asset_registry.request_mesh<sbx::models::mesh>("helmet", "res://meshes/helmet/helmet.gltf");

  asset_registry.request_image("helmet_albedo", "res://meshes/helmet/textures/albedo.jpg", sbx::graphics::format::r8g8b8a8_srgb);
  asset_registry.request_image("helmet_ao", "res://meshes/helmet/textures/ao.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  asset_registry.request_image("helmet_emissive", "res://meshes/helmet/textures/emissive.jpg", sbx::graphics::format::r8g8b8a8_srgb);
  asset_registry.request_image("helmet_mr", "res://meshes/helmet/textures/mr.jpg", sbx::graphics::format::r8g8b8a8_unorm);
  asset_registry.request_image("helmet_normal", "res://meshes/helmet/textures/normal.jpg", sbx::graphics::format::r8g8b8a8_unorm);

  asset_registry.request_image("tree_bark_albedo", "res://meshes/tree/textures/bark/albedo.png", sbx::graphics::format::r8g8b8a8_srgb);
  asset_registry.request_image("tree_bark_normal", "res://meshes/tree/textures/bark/normal.png", sbx::graphics::format::r8g8b8a8_unorm);
  asset_registry.request_image("tree_bark_arm", "res://meshes/tree/textures/bark/arm.png", sbx::graphics::format::r8g8b8a8_unorm);

  asset_registry.request_image("tree_leaves_albedo", "res://meshes/tree/textures/leaves/albedo.png", sbx::graphics::format::r8g8b8a8_srgb);
  asset_registry.request_image("tree_leaves_normal", "res://meshes/tree/textures/leaves/normal.png", sbx::graphics::format::r8g8b8a8_unorm);
  asset_registry.request_image("tree_leaves_arm", "res://meshes/tree/textures/leaves/arm.png", sbx::graphics::format::r8g8b8a8_unorm);

  asset_registry.request_mesh<sbx::models::mesh>("tree", "res://meshes/tree/tree.gltf");

  asset_registry.request_cube_image("skybox", "res://skyboxes/hdr/clouds", std::string{".hdr"}, sbx::graphics::format::r32g32b32a32_sfloat);
  // asset_registry.request_cube_image("skybox", "res://skyboxes/clouds");

  _generate_brdf(512);
  _generate_irradiance(64);
  _generate_prefiltered(512);

  // Helmet

  auto helmet = graph.create_node("Helmet");

  auto& helmet_transform = graph.get_component<sbx::scenes::transform>(helmet);
  helmet_transform.set_position(sbx::math::vector3{0.0f, 5.0f, 0.0f});
  helmet_transform.set_scale(sbx::math::vector3{1.0f, 1.0f, 1.0f});

  auto& helmet_material = asset_registry.request_material<sbx::models::material>("helmet");
  helmet_material.base_color = sbx::math::color::white();
  helmet_material.alpha = sbx::models::alpha_mode::opaque;
  helmet_material.metallic_factor = 1.0f;
  helmet_material.roughness_factor = 1.0f;
  helmet_material.occlusion_strength = 1.0f;
  helmet_material.emissive_factor = sbx::math::vector4{1.0f, 1.0f, 1.0f, 1.0f};
  helmet_material.albedo.image = asset_registry.get_image("helmet_albedo");
  helmet_material.normal.image = asset_registry.get_image("helmet_normal");
  helmet_material.metallic_roughness.image = asset_registry.get_image("helmet_mr");
  helmet_material.occlusion.image = asset_registry.get_image("helmet_ao");
  helmet_material.emissive.image = asset_registry.get_image("helmet_emissive");

  graph.add_component<sbx::scenes::static_mesh>(helmet, asset_registry.get_mesh("helmet"), asset_registry.get_material("helmet"));

  // Base

  auto base = graph.create_node("Base");

  auto& base_transform = graph.get_component<sbx::scenes::transform>(base);
  base_transform.set_position(sbx::math::vector3{0.0f, 0.0f, 0.0f});
  base_transform.set_scale(sbx::math::vector3{100.0f, 0.2f, 100.0f});

  auto& base_material = asset_registry.request_material<sbx::models::material>("base");
  base_material.albedo.image = asset_registry.get_image("floor_albedo");
  base_material.albedo.anisotropy = 16.0f;
  base_material.normal.image = asset_registry.get_image("floor_normal");
  base_material.normal.anisotropy = 16.0f;
  base_material.metallic_roughness.image = asset_registry.get_image("floor_arm");
  base_material.metallic_roughness.anisotropy = 16.0f;
  base_material.occlusion.image = asset_registry.get_image("floor_ao");
  base_material.occlusion.anisotropy = 16.0f;
  base_material.height.image = asset_registry.get_image("floor_height");
  base_material.height.anisotropy = 16.0f;
  base_material.uv_scale = sbx::math::vector2{15, 15};
  base_material.alpha = sbx::models::alpha_mode::opaque;
  base_material.metallic_factor = 0.0f;
  base_material.roughness_factor = 1.0f;
  base_material.occlusion_strength = 1.0f;
  base_material.specular_factor = 0.0f;

  graph.add_component<sbx::scenes::static_mesh>(base, asset_registry.get_mesh("cube"), asset_registry.get_material("base"));

  auto& base_rigidbody = graph.add_component<sbx::physics::rigidbody>(base);
  base_rigidbody.set_is_static(true);

  auto& base_collider = graph.add_component<sbx::physics::shape_collider>(base, sbx::physics::box{sbx::math::vector3{50.0f, 0.1f, 50.0f}});

  auto& cube_material = asset_registry.request_material<sbx::models::material>("cube");
  cube_material.base_color = sbx::math::color::white();
  cube_material.alpha = sbx::models::alpha_mode::opaque;
  cube_material.metallic_factor = 0.0f;
  cube_material.roughness_factor = 1.0f;
  cube_material.occlusion_strength = 1.0f;
  cube_material.specular_factor = 0.0f;

  // Tree

  auto tree = graph.create_node("Tree");

  auto& tree_transform = graph.get_component<sbx::scenes::transform>(tree);
  tree_transform.set_position(sbx::math::vector3{2.0f, 0.0f, 2.0f});

  auto& tree_bark_material = asset_registry.request_material<sbx::models::material>("tree_bark");
  tree_bark_material.albedo.image = asset_registry.get_image("tree_bark_albedo");
  tree_bark_material.normal.image = asset_registry.get_image("tree_bark_normal");
  tree_bark_material.metallic_roughness.image = asset_registry.get_image("tree_bark_arm");
  tree_bark_material.alpha = sbx::models::alpha_mode::opaque;
  tree_bark_material.metallic_factor = 0.0f;
  tree_bark_material.roughness_factor = 1.0f;
  tree_bark_material.occlusion_strength = 1.0f;
  tree_bark_material.specular_factor = 0.0f;
  tree_bark_material.sway_speed = 0.8f;
  tree_bark_material.sway_strength = 0.04f;
  tree_bark_material.sway_falloff_exponent = 3.0f;

  auto& tree_leaves_material = asset_registry.request_material<sbx::models::material>("tree_leaves");
  tree_leaves_material.albedo.image = asset_registry.get_image("tree_leaves_albedo");
  tree_leaves_material.normal.image = asset_registry.get_image("tree_leaves_normal");
  tree_leaves_material.metallic_roughness.image = asset_registry.get_image("tree_leaves_arm");
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
    sbx::scenes::static_mesh::submesh{0, asset_registry.get_material("tree_bark")},
    sbx::scenes::static_mesh::submesh{1, asset_registry.get_material("tree_leaves")}
  };

  graph.add_component<sbx::scenes::static_mesh>(tree, asset_registry.get_mesh("tree"), tree_submeshes);

  // Camera
  
  auto& filesystem_module = sbx::core::engine::get_module<sbx::filesystem::filesystem_module>();

  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  const auto dotnet_dir = filesystem_module.native_path_of(std::string{"engine://dotnet"});

  auto core_assembly_path = std::filesystem::path{dotnet_dir / "editor/Editor.dll"};
  
  scripting_module.load_assembly(core_assembly_path.string());
  
  auto camera_node = environment.camera();

  auto& camera_transform = graph.get_component<sbx::scenes::transform>(camera_node);
  camera_transform.set_position(sbx::math::vector3{2.0f, 7.0f, 2.0f});
  camera_transform.look_at(helmet_transform.position());

  auto& skybox = graph.add_component<sbx::scenes::skybox>(camera_node);
  skybox.cube_image = asset_registry.get_cube_image("skybox");
  skybox.brdf_image = _brdf;
  skybox.irradiance_image = _irradiance;
  skybox.prefiltered_image = _prefiltered;

  scripting_module.instantiate(camera_node, "Editor.CameraController");

  auto& devices_module = sbx::core::engine::get_module<sbx::devices::devices_module>();

  auto& window = devices_module.window();

  window.set_title(scene.name());

  window.on_window_closed_signal() += [this]([[maybe_unused]] const auto& event){
    sbx::core::engine::quit();
  };
}

application::~application() {

}

auto application::update() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto& asset_registry = scenes_module.asset_registry();
  auto& graph = scene.graph();

  if (sbx::devices::input::is_key_pressed(sbx::devices::key::space)) {
    auto cube = graph.create_node("Cube");

    auto axis = sbx::math::vector3::normalized(sbx::math::vector3{sbx::math::random::next<std::float_t>(-1.0f, 1.0f), sbx::math::random::next<std::float_t>(-1.0f, 1.0f), sbx::math::random::next<std::float_t>(-1.0f, 1.0f)});
    auto angle = sbx::math::angle{sbx::math::radian{sbx::math::random::next<std::float_t>(sbx::math::radian::min, sbx::math::radian::max)}};

    auto& cube_transform = graph.get_component<sbx::scenes::transform>(cube);
    cube_transform.set_position(sbx::math::vector3{0.0f, 10.0f, 0.0f});
    cube_transform.set_scale(sbx::math::vector3{1.0f, 1.0f, 1.0f});
    cube_transform.set_rotation(axis, angle);

    graph.add_component<sbx::scenes::static_mesh>(cube, asset_registry.get_mesh("cube"), asset_registry.get_material("cube"));

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

auto application::_generate_brdf(const std::uint32_t size) -> void {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  const auto& logical_device = graphics_module.logical_device();

  const auto& graphics_queue = logical_device.queue<sbx::graphics::queue::type::graphics>();
  const auto& compute_queue = logical_device.queue<sbx::graphics::queue::type::compute>();

  _brdf = graphics_module.add_resource<sbx::graphics::image2d>(sbx::math::vector2u{size}, sbx::graphics::format::r16g16_sfloat, sbx::graphics::filter::linear, sbx::graphics::address_mode::repeat);

  auto timer = sbx::utility::timer{};

  auto& brdf = graphics_module.get_resource<sbx::graphics::image2d>(_brdf);

  auto initial_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

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

  auto compute_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::compute, true};

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

  auto pipeline = sbx::graphics::compute_pipeline{"engine://shaders/ibl/brdf"};

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
    auto final_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

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

  sbx::utility::logger<"editor">::info("Generated 'brdf' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
}

auto application::_generate_irradiance(const std::uint32_t size) -> void {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& asset_registry = scenes_module.asset_registry();

  auto& scene = scenes_module.active_scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  const auto& logical_device = graphics_module.logical_device();
  const auto& graphics_queue = logical_device.queue<sbx::graphics::queue::type::graphics>();
  const auto& compute_queue = logical_device.queue<sbx::graphics::queue::type::compute>();

  _irradiance = graphics_module.add_resource<sbx::graphics::cube_image>(sbx::math::vector2u{size}, VK_FORMAT_R32G32B32A32_SFLOAT);

  auto timer = sbx::utility::timer{};

  auto& irradiance = graphics_module.get_resource<sbx::graphics::cube_image>(_irradiance);
  auto& skybox = graphics_module.get_resource<sbx::graphics::cube_image>(asset_registry.get_cube_image("skybox"));

  auto initial_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

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

  auto compute_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::compute, true};

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

  auto pipeline = sbx::graphics::compute_pipeline{"engine://shaders/ibl/irradiance"};

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
    auto final_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

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

  sbx::utility::logger<"editor">::info("Generated 'irradiance' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
}

auto application::_generate_prefiltered(uint32_t size) -> void {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& asset_registry = scenes_module.asset_registry();

  auto& scene = scenes_module.active_scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  const auto& logical_device = graphics_module.logical_device();
  const auto& graphics_queue = logical_device.queue<sbx::graphics::queue::type::graphics>();
  const auto& compute_queue = logical_device.queue<sbx::graphics::queue::type::compute>();

  _prefiltered = graphics_module.add_resource<sbx::graphics::cube_image>(sbx::math::vector2u{size}, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLE_COUNT_1_BIT, true, true);

  auto timer = sbx::utility::timer{};

  auto& prefiltered = graphics_module.get_resource<sbx::graphics::cube_image>(_prefiltered);
  auto& skybox = graphics_module.get_resource<sbx::graphics::cube_image>(asset_registry.get_cube_image("skybox"));

  auto initial_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

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

  auto compute_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::compute, true};

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

  auto pipeline = sbx::graphics::compute_pipeline{"engine://shaders/ibl/prefiltered"};
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
    auto final_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

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

  sbx::utility::logger<"editor">::info("Generated 'prefiltered' with {} mips in {:.2f}ms", prefiltered.mip_levels(), sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
}

} // namespace editor
