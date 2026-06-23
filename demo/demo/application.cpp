// SPDX-License-Identifier: MIT
#include <demo/application.hpp>

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

#include <demo/renderer.hpp>

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

#include <libsbx/physics/mesh_collider.hpp>
#include <libsbx/physics/shape_collider.hpp>
#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/physics_module.hpp>

#include <libsbx/ui/ui_module.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/audio/audio_module.hpp>
#include <libsbx/sprites/sprites_module.hpp>

#include <libsbx/models/mesh.hpp>
#include <libsbx/models/material.hpp>

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

  auto scene_name = std::string{"res://scenes/scene.yaml"};

  if (auto scene = cli.argument<std::string>("scene"); scene) {
    scene_name = *scene;
  }

  auto& scene = scenes_module.load_scene("Scene", scene_name);
  auto& graph = scene.graph();
  auto& environment = scene.environment();

  auto& filesystem_module = sbx::core::engine::get_module<sbx::filesystem::filesystem_module>();

  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  const auto dotnet_dir = filesystem_module.native_path_of(std::string{"engine://dotnet"});

  auto core_assembly_path = std::filesystem::path{dotnet_dir / "demo/Demo.dll"};

  scripting_module.load_assembly(core_assembly_path.string());

  // Asset helpers

  auto register_material = [&assets_module](sbx::models::material&& material) -> sbx::math::uuid {
    return assets_module.add_runtime_asset(std::make_unique<sbx::models::material>(std::move(material)));
  };

  const auto sphere_mesh = assets_module.load_asset("res://meshes/sphere/sphere.gltf");

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

      const auto material_id = register_material(std::move(material));

      graph.add_component<sbx::scenes::static_mesh>(sphere, sphere_mesh, material_id);

      auto& sphere_transform = graph.get_component<sbx::scenes::transform>(sphere);
      sphere_transform.set_position(sbx::math::vector3{x * 3, y * 3 + 5, 0.0f});
      sphere_transform.set_scale(sbx::math::vector3{1.0f, 1.0f, 1.0f});
    }
  }

  auto& devices_module = sbx::core::engine::get_module<sbx::devices::devices_module>();

  auto& window = devices_module.window();

  window.on_window_closed_signal() += [this]([[maybe_unused]] const auto& event){
    sbx::core::engine::quit();
  };

  // The skybox and its IBL set come from the loaded scene's skybox component
  // (skybox.environment is authored in the scene file and loaded as an environment_map asset).

  // Camera
  auto camera_node = environment.camera();

  scripting_module.instantiate(camera_node, "Demo.EditorCameraController");

  sbx::utility::logger<"demo">::info("string id: {}", sbx::utility::string_id<"foobar">());
}

auto application::update() -> void {
  SBX_PROFILE_SCOPE("application update");

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& scene = scenes_module.active_scene();
  auto& graph = scene.graph();
  auto& environment = scene.environment();

  const auto delta_time = sbx::core::engine::delta_time();

  _rotation += sbx::math::degree{45} * delta_time;

  if (!scenes_module.has_active_scene()) {
    return;
  }

  auto query = graph.query<const sbx::scenes::static_mesh>();

  for (auto&& [node, static_mesh] : query.each()) {
    const auto& mesh = assets_module.get_loaded<sbx::models::mesh>(static_mesh.mesh_id());
    const auto world = graph.world_transform(node);

    for (const auto& submesh : static_mesh.submeshes()) {
      const auto base_index = mesh.find_base_submesh_index(submesh.index).value_or(submesh.index);
      scenes_module.add_debug_box(world, mesh.submesh_bounds(base_index), sbx::math::color::green());
    }
  }

  if (sbx::devices::input::is_key_pressed(sbx::devices::key::f5)) {
    _debug_frustum_active = !_debug_frustum_active;

    if (_debug_frustum_active) {
      // Snap to current camera position/orientation
      const auto camera_node = environment.camera();
      _debug_frustum_position = graph.world_position(camera_node);

      const auto world = graph.world_transform(camera_node);
      const auto forward = -sbx::math::vector3::normalized(sbx::math::vector3{world[2]});

      _debug_frustum_yaw = std::atan2(forward.x(), forward.z());
      _debug_frustum_pitch = std::asin(std::clamp(forward.y(), -1.0f, 1.0f));
    }
  }

  auto& renderer = graphics_module.renderer();
  auto culling_task = renderer.task<sbx::models::frustum_culling_task>();

  if (!_debug_frustum_active) {
    culling_task->clear_debug_view_projection();
    return;
  }

  // Move the debug frustum
  const auto dt = sbx::core::engine::delta_time().value();
  const auto speed = 10.0f * dt;
  const auto rotate_speed = 1.5f * dt;

  if (sbx::devices::input::is_key_down(sbx::devices::key::kp_4)) {
    _debug_frustum_yaw += rotate_speed;
  }

  if (sbx::devices::input::is_key_down(sbx::devices::key::kp_6)) {
    _debug_frustum_yaw -= rotate_speed;
  }

  if (sbx::devices::input::is_key_down(sbx::devices::key::kp_8)) {
    _debug_frustum_pitch += rotate_speed;
  }

  if (sbx::devices::input::is_key_down(sbx::devices::key::kp_2)) {
    _debug_frustum_pitch -= rotate_speed;
  }

  _debug_frustum_pitch = std::clamp(_debug_frustum_pitch, -1.5f, 1.5f);

  const auto forward = sbx::math::vector3{
    std::sin(_debug_frustum_yaw) * std::cos(_debug_frustum_pitch),
    std::sin(_debug_frustum_pitch),
    std::cos(_debug_frustum_yaw) * std::cos(_debug_frustum_pitch)
  };

  const auto right = sbx::math::vector3::normalized(sbx::math::vector3::cross(forward, sbx::math::vector3::up));

  if (sbx::devices::input::is_key_down(sbx::devices::key::kp_add)) {
    _debug_frustum_position += forward * speed;
  }

  if (sbx::devices::input::is_key_down(sbx::devices::key::kp_subtract)) {
    _debug_frustum_position -= forward * speed;
  }

  if (sbx::devices::input::is_key_down(sbx::devices::key::kp_7)) {
    _debug_frustum_position += sbx::math::vector3::up * speed;
  }

  if (sbx::devices::input::is_key_down(sbx::devices::key::kp_1)) {
    _debug_frustum_position -= sbx::math::vector3::up * speed;
  }

  if (sbx::devices::input::is_key_down(sbx::devices::key::kp_9)) {
    _debug_frustum_position += right * speed;
  }

  if (sbx::devices::input::is_key_down(sbx::devices::key::kp_3)) {
    _debug_frustum_position -= right * speed;
  }

  // Build VP
  const auto camera_node = environment.camera();
  const auto& cam = graph.get_component<sbx::scenes::camera>(camera_node);
  const auto aspect = static_cast<std::float_t>(environment.render_target_size().x()) / static_cast<std::float_t>(environment.render_target_size().y());

  const auto target = _debug_frustum_position + forward;
  const auto view = sbx::math::matrix4x4::look_at(_debug_frustum_position, target, sbx::math::vector3::up);
  const auto projection = cam.projection(aspect);
  const auto vp = projection * view;

  culling_task->set_debug_view_projection(vp);

  // Visualize the frustum
  scenes_module.add_debug_frustum(view, projection, sbx::math::color{1.0f, 0.5f, 0.0f, 1.0f});
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

} // namespace demo
