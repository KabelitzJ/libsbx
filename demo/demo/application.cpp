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

static auto intersect_ray_plane(const sbx::math::ray& r, const sbx::math::plane& p) -> std::optional<sbx::math::vector3> {
  const auto denom = sbx::math::vector3::dot(p.normal(), r.direction());

  if (std::abs(denom) < 1e-6f) {
    return std::nullopt;
  }

  const auto t = -p.distance_to_point(r.origin()) / denom;

  if (t < 0.0f) {
    return std::nullopt;
  }

  return r.point_at(t);
}

static const auto settings = dual_grid<grid_cell_data>::settings{
  .rings = 5u,
  .ring_distance = 25.0f,
  .seed = 1643u,
  .merge_probability = 0.5f,
  .split_quads_to_4 = true,
  .split_leftover_tris_to_3 = true,
  .relax = true,
  .relax_iterations = 80u,
  .relax_lambda = 0.45f,
  .relax_mu = -0.50f,
  .relax_add_diagonals = true,
  .build_dual = true
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

  graphics_module.set_renderer<renderer>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.load_scene("res://scenes/scene.yaml");

  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  // Textures
  scene.add_cube_image("skybox", "res://skyboxes/clouds2");

  _generate_brdf(512);
  _generate_irradiance(64);
  _generate_prefiltered(512);

  // Meshes

  // Animations

  // Window

  auto& devices_module = sbx::core::engine::get_module<sbx::devices::devices_module>();

  auto& window = devices_module.window();

  window.on_window_closed_signal() += [this]([[maybe_unused]] const auto& event){
    sbx::core::engine::quit();
  };

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

  _rotation += sbx::math::degree{45} * delta_time;

  for (const auto& v : _grid.vertices()) {
    scenes_module.add_debug_sphere(v.position, 0.25f, v.is_fixed ? sbx::math::color::yellow() : sbx::math::color::blue(), 12);
  }

  for (const auto& q : _grid.quads()) {
    const auto& a = _grid.vertex_at(q.a).position;
    const auto& b = _grid.vertex_at(q.b).position;
    const auto& c = _grid.vertex_at(q.c).position;
    const auto& d = _grid.vertex_at(q.d).position;

    scenes_module.add_debug_line(a, b, sbx::math::color::green());
    scenes_module.add_debug_line(b, c, sbx::math::color::green());
    scenes_module.add_debug_line(c, d, sbx::math::color::green());
    scenes_module.add_debug_line(d, a, sbx::math::color::green());
  }

  for (const auto& dv : _grid.dual_vertices()) {
    scenes_module.add_debug_sphere(dv.position, 0.3f, dv.is_boundary ? sbx::math::color::red() : sbx::math::color::cyan(), 12);
  }

  for (const auto& de : _grid.dual_edges()) {
    const auto& a = _grid.dual_vertex_at(de.a).position;
    const auto& b = _grid.dual_vertex_at(de.b).position;

    scenes_module.add_debug_line(a, b, de.is_boundary ? sbx::math::color::orange() : sbx::math::color::white());
  }

  for (auto vi = std::uint32_t{0u}; vi < _grid.vertices().size(); ++vi) {
    const auto cell = _grid.dual_cell_ccw(vi);

    for (auto i = std::size_t{0u}; i < cell.size(); ++i) {
      const auto a = _grid.dual_vertex_at(cell[i]).position;
      const auto b = _grid.dual_vertex_at(cell[(i + 1u) % cell.size()]).position;

      scenes_module.add_debug_line(a, b, sbx::math::color::magenta());
    }
  }

  static auto point = std::optional<sbx::math::vector3>{};

  if (sbx::devices::input::is_mouse_button_pressed(sbx::devices::mouse_button::left)) {
    auto screen_position = sbx::devices::input::mouse_position();

    auto ray = scene.screen_point_to_ray(screen_position);

    const auto ground = sbx::math::plane{sbx::math::vector3::up, 0.0f};

    if (const auto hit = intersect_ray_plane(ray, ground); hit) {
      point = *hit;
    }
  }

  if (point) {
    scenes_module.add_debug_sphere(*point, 0.6f, sbx::math::color::red(), 16);
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
