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

#include <libsbx/sprites/sprite_subrenderer.hpp>

namespace demo {

static auto intersect_ray_plane(const sbx::math::ray& ray, const sbx::math::plane& plane) -> std::optional<sbx::math::vector3> {
  const auto denominator = sbx::math::vector3::dot(plane.normal(), ray.direction());

  if (sbx::math::comparision_traits<std::float_t>::equal(denominator, 0.0f)) {
    return std::nullopt;
  }

  const auto t = -plane.distance_to_point(ray.origin()) / denominator;
  
  if (t < 0.0f) {
    return std::nullopt;
  }

  return ray.point_at(t);
}

static const auto settings = application::grid_type::settings{
  .rings = 15u,
  .ring_distance = 10.0f,
  .seed = 19517357u,
  .merge_probability = 0.4f,
  .relax_iterations = 40u,
  .relax_lambda = 0.45f,
  .relax_mu = -0.50f
};

application::application()
: sbx::core::application{},
  _rotation{sbx::math::degree{0}},
  _grid{settings} { 
  // Renderer
  const auto& cli = sbx::core::engine::cli();

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (auto assets = cli.argument<std::string>("assets"); assets) {
    assets_module.set_asset_root(*assets);
  } else {
    assets_module.set_asset_root("demo/assets");
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& renderer = graphics_module.set_renderer<demo::renderer>();
  renderer.update_dual_grid_data(_grid);

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.load_scene("res://scenes/scene.yaml");

  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  // Textures
  scene.add_image("base", "res://textures/base.png");

  scene.add_cube_image("skybox", "res://skyboxes/clouds2");

  _generate_brdf(512);
  _generate_irradiance(64);
  _generate_prefiltered(512);

  // Meshes
  scene.add_mesh<sbx::models::mesh>("corner", "res://meshes/terrain/corner/corner.gltf");
  scene.add_mesh<sbx::models::mesh>("diagonal", "res://meshes/terrain/diagonal/diagonal.gltf");
  scene.add_mesh<sbx::models::mesh>("full", "res://meshes/terrain/full/full.gltf");
  scene.add_mesh<sbx::models::mesh>("half", "res://meshes/terrain/half/half.gltf");
  scene.add_mesh<sbx::models::mesh>("three_corner", "res://meshes/terrain/three_corner/three_corner.gltf");

  // Materials

  auto& base_material = scene.add_material<sbx::models::material>("base");
  base_material.albedo.image = scene.get_image("base");

  // Animations

  // Window

  auto& devices_module = sbx::core::engine::get_module<sbx::devices::devices_module>();

  auto& window = devices_module.window();

  window.on_window_closed_signal() += [this]([[maybe_unused]] const auto& event){
    sbx::core::engine::quit();
  };

  auto corner = scene.create_node("Corner", sbx::scenes::transform{sbx::math::vector3{-10.0f, 10.0f, 0.0f}});
  scene.add_component<sbx::scenes::static_mesh>(corner, sbx::scenes::static_mesh{scene.get_mesh("corner"), scene.get_material("base")});

  auto diagonal = scene.create_node("Diagonal", sbx::scenes::transform{sbx::math::vector3{0.0f, 10.0f, 0.0f}});
  scene.add_component<sbx::scenes::static_mesh>(diagonal, sbx::scenes::static_mesh{scene.get_mesh("diagonal"), scene.get_material("base")});

  auto full = scene.create_node("Full", sbx::scenes::transform{sbx::math::vector3{10.0f, 10.0f, 0.0f}});
  scene.add_component<sbx::scenes::static_mesh>(full, sbx::scenes::static_mesh{scene.get_mesh("full"), scene.get_material("base")});

  auto half = scene.create_node("Half", sbx::scenes::transform{sbx::math::vector3{20.0f, 10.0f, 0.0f}});
  scene.add_component<sbx::scenes::static_mesh>(half, sbx::scenes::static_mesh{scene.get_mesh("half"), scene.get_material("base")});

  auto three_corner = scene.create_node("ThreeCorner", sbx::scenes::transform{sbx::math::vector3{30.0f, 10.0f, 0.0f}});
  scene.add_component<sbx::scenes::static_mesh>(three_corner, sbx::scenes::static_mesh{scene.get_mesh("three_corner"), scene.get_material("base")});

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
  SBX_PROFILE_SCOPE("application::update");

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

  static auto selected_cell = grid_type::invalid_id;

  if (sbx::devices::input::is_mouse_button_pressed(sbx::devices::mouse_button::left)) {
    auto screen_position = sbx::devices::input::mouse_position();

    auto ray = scene.screen_point_to_ray(screen_position);

    const auto ground = sbx::math::plane{sbx::math::vector3::up, 0.0f};

    if (const auto hit = intersect_ray_plane(ray, ground); hit) {
      selected_cell = _grid.pick_main_face_at(*hit);

      if (selected_cell != grid_type::invalid_id) {
        _rebuild_terrain_tiles();
      }
    }
  }

  for (const auto quad : _grid.dual_quads()) {
    const auto& a = _grid.dual_vertex_at(quad.a).position;
    const auto& b = _grid.dual_vertex_at(quad.b).position;
    const auto& c = _grid.dual_vertex_at(quad.c).position;
    const auto& d = _grid.dual_vertex_at(quad.d).position;

    scenes_module.add_debug_line(a, b, sbx::math::color::white());
    scenes_module.add_debug_line(b, c, sbx::math::color::white());
    scenes_module.add_debug_line(c, d, sbx::math::color::white());
    scenes_module.add_debug_line(d, a, sbx::math::color::white());
  }

  for (const auto& e : _grid.main_edges()) {
    const auto& a = _grid.main_vertex_at(e.a).position;
    const auto& b = _grid.main_vertex_at(e.b).position;

    scenes_module.add_debug_line(a, b, sbx::math::color::cyan());
  }

  if (selected_cell != grid_type::invalid_id) {
    const auto poly = _grid.main_cell_ccw(selected_cell);

    for (auto i = std::size_t{0u}; i < poly.size(); ++i) {
      const auto a_id = poly[i];
      const auto b_id = poly[(i + 1u) % poly.size()];

      const auto& a = _grid.main_vertex_at(a_id).position;
      const auto& b = _grid.main_vertex_at(b_id).position;

      scenes_module.add_debug_line(a, b, sbx::math::color::yellow());
    }
  }
}

auto application::fixed_update() -> void {

}

auto application::_rebuild_terrain_tiles() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.scene();


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
