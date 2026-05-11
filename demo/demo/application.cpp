// SPDX-License-Identifier: MIT
#include <demo/application.hpp>

#include <nlohmann/json.hpp>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/reflection/reflection.hpp>

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

#include <libsbx/ui/ui_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/audio/audio_module.hpp>
#include <libsbx/sprites/sprites_module.hpp>
#include <libsbx/ui/ui_module.hpp>

#include <demo/terrain/terrain_module.hpp>

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

  auto& filesystem_module = sbx::core::engine::get_module<sbx::filesystem::filesystem_module>();

  const auto engine_data_dir = filesystem_module.native_path_of(std::string{"engine://"});
  const auto editor_data_dir = engine_data_dir / "demo";

  if (!std::filesystem::exists(editor_data_dir)) {
    std::filesystem::create_directories(editor_data_dir);
  }

  filesystem_module.create_filesystem<sbx::filesystem::native_filesystem>(sbx::filesystem::alias{"demo://"}, editor_data_dir.generic_string());

  auto& audio_module = sbx::core::engine::get_module<sbx::audio::audio_module>();

  auto& terrain = sbx::core::engine::get_module<demo::terrain_module>();
  terrain.load();

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  graphics_module.set_renderer<demo::renderer>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& asset_registry = scenes_module.asset_registry();

  auto scene_name = std::string{"res://scenes/scene.yaml"};

  if (auto scene = cli.argument<std::string>("scene"); scene) {
    scene_name = *scene;
  }

  auto& scene = scenes_module.create_scene("Scene");
  auto& graph = scene.graph();
  auto& environment = scene.environment();

  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  const auto dotnet_dir = filesystem_module.native_path_of(std::string{"engine://dotnet"});

  auto core_assembly_path = std::filesystem::path{dotnet_dir / "demo/Demo.dll"};
  
  scripting_module.load_assembly(core_assembly_path.string());

  // Textures

  asset_registry.request_image("roboto_atlas", "res://fonts/roboto_atlas.png", sbx::graphics::format::r8g8b8a8_srgb);

  asset_registry.request_cube_image("skybox", "res://skyboxes/hdr/clouds", std::string{".hdr"}, sbx::graphics::format::r32g32b32a32_sfloat);

  _generate_brdf(512);
  _generate_irradiance(64);
  _generate_prefiltered(512);

  // Meshes
  asset_registry.request_mesh<sbx::models::mesh>("sphere", "res://meshes/sphere/sphere.gltf");

  // Materials

  // Animations

  // Window

  // auto spheres = graph.create_node(fmt::format("Spheres"));

  // auto& spheres_transform = graph.get_component<sbx::scenes::transform>(spheres);
  // spheres_transform.set_position(sbx::math::vector3{0, 0, -15});

  // for (auto y = 0; y < 5; ++y) {
  //   for (auto x = 0; x < 5; ++x) {
  //     auto sphere = graph.create_child_node(spheres, fmt::format("Sphere{}{}", x, y));

  //     const auto material_name = fmt::format("sphere_{}_{}_material", x, y);

  //     auto& material = asset_registry.request_material<sbx::models::material>(material_name);
  //     material.base_color = sbx::math::color::white();
  //     material.alpha = sbx::models::alpha_mode::opaque;
  //     material.metallic_factor = 0.2f * x;
  //     material.roughness_factor = 0.2f * y;
  //     material.occlusion_strength = 1.0f;

  //     graph.add_component<sbx::scenes::static_mesh>(sphere, asset_registry.get_mesh("sphere"), asset_registry.get_material(material_name));

  //     auto& sphere_transform = graph.get_component<sbx::scenes::transform>(sphere);
  //     sphere_transform.set_position(sbx::math::vector3{x * 3, y * 3 + 5, 0.0f});
  //     sphere_transform.set_scale(sbx::math::vector3{1.0f, 1.0f, 1.0f});
  //   }
  // }

  auto& devices_module = sbx::core::engine::get_module<sbx::devices::devices_module>();

  auto& window = devices_module.window();

  window.on_window_closed_signal() += [this]([[maybe_unused]] const auto& event){
    sbx::core::engine::quit();
  };

  // // UI

  // _font = sbx::ui::load_font(asset_registry.get_image("roboto_atlas"), "demo/assets/fonts/roboto_atlas.json");

  // _build_ui();
  

  // Camera
  auto camera_node = environment.camera();

  // graph.add_component<sbx::scenes::skybox>(camera_node, assets.get_cube_image("skybox"), _brdf, _irradiance, _prefiltered, sbx::math::color::white());

  auto& skybox = graph.add_component<sbx::scenes::skybox>(camera_node);
  skybox.cube_image = asset_registry.get_cube_image("skybox");
  skybox.brdf_image = _brdf;
  skybox.irradiance_image = _irradiance;
  skybox.prefiltered_image = _prefiltered;

  scripting_module.instantiate(camera_node, "Demo.EditorCameraController");

  sbx::utility::logger<"demo">::info("string id: {}", sbx::utility::string_id<"foobar">());
}

auto _read_object_id(const sbx::graphics::image2d& image, std::uint32_t pixel_x, std::uint32_t pixel_y) -> std::optional<std::uint32_t> {
  auto staging = sbx::graphics::buffer{sizeof(std::uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

  auto command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

  const auto subresource_range = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  // Acquire: wait for color writes, transition to TRANSFER_SRC

  auto pre_barrier = VkImageMemoryBarrier2{};
  pre_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

  pre_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  pre_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

  pre_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  pre_barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

  pre_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  pre_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

  pre_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  pre_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  pre_barrier.image = image.handle();

  pre_barrier.subresourceRange = subresource_range;

  auto pre_dependency = VkDependencyInfo{};
  pre_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  pre_dependency.imageMemoryBarrierCount = 1;
  pre_dependency.pImageMemoryBarriers = &pre_barrier;

  vkCmdPipelineBarrier2(command_buffer, &pre_dependency);

  // Copy

  auto region = VkBufferImageCopy{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = VkOffset3D{static_cast<std::int32_t>(pixel_x), static_cast<std::int32_t>(pixel_y), 0};
  region.imageExtent = VkExtent3D{1, 1, 1};

  vkCmdCopyImageToBuffer(command_buffer, image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.handle(), 1, &region);

  // Release: transition back to SHADER_READ_ONLY

  auto post_barrier = VkImageMemoryBarrier2{};
  post_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

  post_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  post_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  post_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  post_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

  post_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  post_barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

  post_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  post_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  post_barrier.image = image.handle();
  post_barrier.subresourceRange = subresource_range;

  auto post_dependency = VkDependencyInfo{};
  post_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  post_dependency.imageMemoryBarrierCount = 1;
  post_dependency.pImageMemoryBarriers = &post_barrier;

  vkCmdPipelineBarrier2(command_buffer, &post_dependency);

  command_buffer.submit_idle();

  staging.map();

  auto value = std::uint32_t{0};
  std::memcpy(&value, staging.mapped_memory().get(), sizeof(std::uint32_t));

  staging.unmap();

  return value;
}

auto application::update() -> void {
  SBX_PROFILE_SCOPE("application update");

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& devices_module = sbx::core::engine::get_module<sbx::devices::devices_module>();
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  auto& scene = scenes_module.active_scene();
  auto& graph = scene.graph();
  auto& environment = scene.environment();

  const auto delta_time = sbx::core::engine::delta_time();

  _rotation += sbx::math::degree{45} * delta_time;

  if (!scenes_module.has_active_scene()) {
    return;
  }

  _time_accumulator += delta_time;
  _frame_count++;

  if (_time_accumulator >= sbx::units::second{1}) {
    const auto fps = static_cast<std::float_t>(_frame_count) / _time_accumulator.value();
    sbx::utility::logger<"demo">::info("FPS: {:.2f}", fps);
    _time_accumulator = sbx::units::second{0};
    _frame_count = 0u;
  }

  if (sbx::devices::input::is_mouse_button_pressed(sbx::devices::mouse_button::left)) {
    const auto& object_id_image = static_cast<const sbx::graphics::image2d&>(graphics_module.attachment("object_id"));
    const auto& image_size = object_id_image.size();
  
    const auto mouse_position = sbx::devices::input::mouse_position();
  
    const auto id = _read_object_id(object_id_image, mouse_position.x(), mouse_position.y());
  
    if (!id.has_value() || *id == 0xFFFFFFFFu) {
      terrain_module.set_selected_province_id(0xFFFFFFFFu);
    } else {
      if ((*id & 0x80000000u) != 0u) {
        auto province_id = *id & 0x7FFFFFFFu;
        terrain_module.set_selected_province_id(province_id);
      }
    }
  }

}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

auto application::_build_ui() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();
  auto& graph = scene.graph();

  auto ui_node = graph.create_node("HUD");

  auto& canvas = graph.add_component<sbx::ui::canvas>(ui_node);
  canvas.is_enabled = true;

  // Left sidebar - vertical layout
  auto& sidebar = canvas.create<sbx::ui::panel>();
  sidebar.color = {0.1f, 0.1f, 0.12f, 0.95f};
  sidebar.anchor_min = {0.0f, 0.0f};
  sidebar.anchor_max = {0.0f, 1.0f};
  sidebar.offset_min = {0.0f, 0.0f};
  sidebar.offset_max = {220.0f, 0.0f};
  sidebar.sort_order = 100;
  auto& sidebar_layout = sidebar.set_layout<sbx::ui::vertical_layout>(6.0f);
  sidebar_layout.padding = {12.0f, 12.0f, 12.0f, 12.0f};

  // Title
  auto& title = canvas.create<sbx::ui::label>(sidebar);
  title.set_text("Main Menu");
  title.set_font(_font);
  title.set_font_size(24.0f);
  title.color = {1.0f, 1.0f, 1.0f, 1.0f};
  title.sizing.preferred = {0.0f, 32.0f};
  title.sort_order = 101;

  // Menu buttons
  auto make_menu_button = [&](sbx::ui::element& parent, const std::string& text) -> sbx::ui::button& {
    auto& btn = canvas.create<sbx::ui::button>(parent);
    btn.normal_color = {0.2f, 0.2f, 0.25f, 1.0f};
    btn.hovered_color = {0.3f, 0.3f, 0.38f, 1.0f};
    btn.pressed_color = {0.15f, 0.15f, 0.18f, 1.0f};
    btn.color = btn.normal_color;
    btn.sizing.preferred = {0.0f, 36.0f};
    btn.sort_order = 401;

    auto& lbl = canvas.create<sbx::ui::label>(btn);
    lbl.set_text(text);
    lbl.set_font(_font);
    lbl.set_font_size(16.0f);
    lbl.anchor_min = {0.0f, 0.0f};
    lbl.anchor_max = {1.0f, 1.0f};
    lbl.sort_order = 402;

    return btn;
  };

  // Popup

  auto& popup = canvas.create<sbx::ui::panel>();
  popup.color = {0.15f, 0.15f, 0.18f, 0.95f};
  popup.anchor_min = {0.5f, 0.5f};
  popup.anchor_max = {0.5f, 0.5f};
  popup.offset_min = {-150.0f, -80.0f};
  popup.offset_max = {150.0f, 80.0f};
  popup.sort_order = 200;
  popup.is_enabled = false;
  auto& popup_layout = popup.set_layout<sbx::ui::vertical_layout>(8.0f);
  popup_layout.padding = {12.0f, 12.0f, 12.0f, 12.0f};

  auto& popup_title = canvas.create<sbx::ui::label>(popup);
  popup_title.set_text("Settings");
  popup_title.set_font(_font);
  popup_title.set_font_size(20.0f);
  popup_title.sizing.preferred = {0.0f, 28.0f};
  popup_title.sort_order = 201;

  auto& popup_body = canvas.create<sbx::ui::label>(popup);
  popup_body.set_text("Nothing here yet.");
  popup_body.set_font(_font);
  popup_body.set_font_size(14.0f);
  popup_body.color = {0.7f, 0.7f, 0.7f, 1.0f};
  popup_body.sizing.flex = 1.0f;
  popup_body.sort_order = 201;

  auto& close_btn = make_menu_button(popup, "Close");
  close_btn.on_click = [&popup] { popup.is_enabled = false; };

  auto& btn_play = make_menu_button(sidebar, "Play");
  btn_play.on_click = [&popup](){ 
    sbx::utility::logger<"demo">::info("Play clicked");
    popup.is_enabled = true;
  };


  auto& btn_settings = make_menu_button(sidebar, "Settings");
  btn_settings.on_click = [&popup](){ 
    sbx::utility::logger<"demo">::info("Settings clicked");
    popup.is_enabled = true;
  };

  auto& btn_credits = make_menu_button(sidebar, "Credits");
  btn_credits.on_click = [&popup](){
    sbx::utility::logger<"demo">::info("Credits clicked");
    popup.is_enabled = true;
  };

  // Spacer pushes quit to bottom
  auto& spacer = canvas.create<sbx::ui::element>(sidebar);
  spacer.sizing.flex = 1.0f;

  auto& btn_quit = make_menu_button(sidebar, "Quit");
  btn_quit.normal_color = {0.5f, 0.1f, 0.1f, 1.0f};
  btn_quit.hovered_color = {0.7f, 0.15f, 0.15f, 1.0f};
  btn_quit.pressed_color = {0.4f, 0.08f, 0.08f, 1.0f};
  btn_quit.color = btn_quit.normal_color;
  btn_quit.on_click = [] { sbx::core::engine::quit(); };

  // Top bar - horizontal layout
  auto& topbar = canvas.create<sbx::ui::panel>();
  topbar.color = {0.1f, 0.1f, 0.12f, 0.9f};
  topbar.anchor_min = {0.0f, 1.0f};
  topbar.anchor_max = {1.0f, 1.0f};
  topbar.offset_min = {220.0f, -48.0f};
  topbar.offset_max = {0.0f, 0.0f};
  topbar.sort_order = 100;
  auto& topbar_layout = topbar.set_layout<sbx::ui::horizontal_layout>(8.0f);
  topbar_layout.padding = {8.0f, 12.0f, 8.0f, 12.0f};

  // Health bar in topbar
  auto& health_group = canvas.create<sbx::ui::element>(topbar);
  health_group.sizing.preferred = {200.0f, 0.0f};

  auto& health_bg = canvas.create<sbx::ui::panel>(health_group);
  health_bg.color = {0.15f, 0.15f, 0.15f, 1.0f};
  health_bg.anchor_min = {0.0f, 0.0f};
  health_bg.anchor_max = {1.0f, 1.0f};
  health_bg.sort_order = 101;

  auto& health_fill = canvas.create<sbx::ui::panel>(health_bg);
  health_fill.color = {0.2f, 0.8f, 0.2f, 1.0f};
  health_fill.anchor_min = {0.0f, 0.0f};
  health_fill.anchor_max = {0.75f, 1.0f};
  health_fill.offset_min = {2.0f, 2.0f};
  health_fill.offset_max = {-2.0f, -2.0f};
  health_fill.sort_order = 102;

  auto& health_label = canvas.create<sbx::ui::label>(health_bg);
  health_label.set_text("HP: 75%");
  health_label.set_font(_font);
  health_label.set_font_size(14.0f);
  health_label.anchor_min = {0.0f, 0.0f};
  health_label.anchor_max = {1.0f, 1.0f};
  health_label.sort_order = 103;

  // Topbar spacer
  auto& topbar_spacer = canvas.create<sbx::ui::element>(topbar);
  topbar_spacer.sizing.flex = 1.0f;

  // FPS label right-aligned
  auto& fps_label = canvas.create<sbx::ui::label>(topbar);
  fps_label.set_text("FPS: 60");
  fps_label.set_font(_font);
  fps_label.set_font_size(14.0f);
  fps_label.color = {0.6f, 0.6f, 0.6f, 1.0f};
  fps_label.sizing.preferred = {80.0f, 0.0f};
  fps_label.sort_order = 101;

  // Delta label right-aligned
  auto& dt_label = canvas.create<sbx::ui::label>(topbar);
  dt_label.set_text("Delta: 0.33 [ms]");
  dt_label.set_font(_font);
  dt_label.set_font_size(14.0f);
  dt_label.color = {0.6f, 0.6f, 0.6f, 1.0f};
  dt_label.sizing.preferred = {80.0f, 0.0f};
  dt_label.sort_order = 101;

  // Bottom right - inventory grid
  auto& inventory = canvas.create<sbx::ui::panel>();
  inventory.color = {0.1f, 0.1f, 0.12f, 0.9f};
  inventory.anchor_min = {1.0f, 0.0f};
  inventory.anchor_max = {1.0f, 0.0f};
  inventory.offset_min = {-220.0f, 10.0f};
  inventory.offset_max = {-10.0f, 230.0f};
  inventory.sort_order = 100;

  auto& inventory_layout = inventory.set_layout<sbx::ui::grid_layout>(4, 4, 4.0f);
  inventory_layout.padding = {8.0f, 8.0f, 8.0f, 8.0f};

  for (auto i = 0; i < 16; ++i) {
    auto& slot = canvas.create<sbx::ui::panel>(inventory);
    slot.color = {0.2f, 0.2f, 0.25f, 1.0f};
    slot.sort_order = 101;
  }
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

  sbx::utility::logger<"demo">::info("Generated 'brdf' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
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

  sbx::utility::logger<"demo">::info("Generated 'irradiance' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
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

  sbx::utility::logger<"demo">::info("Generated 'prefiltered' with {} mips in {:.2f}ms", prefiltered.mip_levels(), sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));
}

} // namespace demo
