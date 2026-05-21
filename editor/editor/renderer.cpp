// SPDX-License-Identifier: MIT
#include <editor/renderer.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/skybox_subrenderer.hpp>
#include <libsbx/scenes/debug_subrenderer.hpp>
#include <libsbx/scenes/grid_subrenderer.hpp>

#include <libsbx/models/models.hpp>
#include <libsbx/models/static_mesh_material_subrenderer.hpp>
#include <libsbx/models/static_mesh_shadow_subrenderer.hpp>

#include <libsbx/animations/skinning_task.hpp>
#include <libsbx/animations/skinned_mesh_material_subrenderer.hpp>
#include <libsbx/animations/skinned_mesh_shadow_subrenderer.hpp>

#include <libsbx/particles/particle_task.hpp>
#include <libsbx/particles/particle_subrenderer.hpp>

#include <libsbx/post/filters/resolve_filter.hpp>
#include <libsbx/post/filters/blur_filter.hpp>
#include <libsbx/post/filters/fxaa_filter.hpp>
#include <libsbx/post/filters/tonemap_filter.hpp>
#include <libsbx/post/filters/downsample_filter.hpp>
#include <libsbx/post/filters/upsample_filter.hpp>
#include <libsbx/post/filters/ssao_filter.hpp>
#include <libsbx/post/filters/brightness_filter.hpp>

#include <libsbx/gizmos/gizmos_subrenderer.hpp>
#include <libsbx/sprites/sprite_subrenderer.hpp>

#include <editor/application.hpp>

#include <editor/filters/selection_filter.hpp>
#include <editor/editor_subrenderer.hpp>

namespace editor {

renderer::renderer()
: _clear_color{sbx::math::color::white()} {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  // Declare scene viewport
  auto& viewports = graphics_module.viewports();
  const auto extent = graphics_module.surface().current_extent();
  viewports.declare("scene", sbx::math::vector2u{extent.width, extent.height});

  auto scene_viewport = sbx::graphics::viewport::named("scene");

  // Attachments
  auto shadow = create_attachment("shadow", sbx::graphics::attachment::type::image, sbx::math::color::white(), sbx::graphics::format::r32_sfloat, sbx::graphics::blend_state{}, sbx::graphics::filter::nearest, sbx::graphics::address_mode::clamp_to_edge, 4u);
  auto shadow_depth = create_attachment("shadow_depth", sbx::graphics::attachment::type::depth, sbx::math::color::black(), sbx::graphics::format::undefined, sbx::graphics::blend_state{}, sbx::graphics::filter::linear, sbx::graphics::address_mode::repeat, 4u);

  auto depth = create_attachment("depth", sbx::graphics::attachment::type::depth);
  auto albedo = create_attachment("albedo", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_srgb);
  auto position = create_attachment("position", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat);
  auto normal = create_attachment("normal", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::a2b10g10r10_unorm_pack32);
  auto material = create_attachment("material", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
  auto emissive = create_attachment("emissive", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat);
  auto object_id = create_attachment("object_id", sbx::graphics::attachment::type::image, sbx::math::vector4u{0xFFFFFFFFu, 0u, 0u, 0u}, sbx::graphics::format::r32_uint);
  auto linear_depth = create_attachment("linear_depth", sbx::graphics::attachment::type::image, sbx::math::color::white(), sbx::graphics::format::r32_sfloat);

  const auto accum_blend = sbx::graphics::blend_state{
    .color_source = sbx::graphics::blend_factor::one,
    .color_destination = sbx::graphics::blend_factor::one,
    .color_operation = sbx::graphics::blend_operation::add,
    .alpha_source = sbx::graphics::blend_factor::one,
    .alpha_destination = sbx::graphics::blend_factor::one,
    .alpha_operation = sbx::graphics::blend_operation::add,
    .color_write_mask = sbx::graphics::color_component::r | sbx::graphics::color_component::g | sbx::graphics::color_component::b | sbx::graphics::color_component::a
  };

  const auto revealage_blend = sbx::graphics::blend_state{
    .color_source = sbx::graphics::blend_factor::zero,
    .color_destination = sbx::graphics::blend_factor::one_minus_source_color,
    .color_operation = sbx::graphics::blend_operation::add,
    .alpha_source = sbx::graphics::blend_factor::zero,
    .alpha_destination = sbx::graphics::blend_factor::one_minus_source_alpha,
    .alpha_operation = sbx::graphics::blend_operation::add,
    .color_write_mask = sbx::graphics::color_component::r
  };

  auto accumulator = create_attachment("accumulator", sbx::graphics::attachment::type::image, sbx::math::color{0.0f, 0.0f, 0.0f, 0.0f}, sbx::graphics::format::r32g32b32a32_sfloat, accum_blend);
  auto revealage = create_attachment("revealage", sbx::graphics::attachment::type::image, sbx::math::color{1.0f, 0.0f, 0.0f, 0.0f}, sbx::graphics::format::r32_sfloat, revealage_blend);

  const auto resolve_blend = sbx::graphics::blend_state{
    .color_source = sbx::graphics::blend_factor::source_alpha,
    .color_destination = sbx::graphics::blend_factor::one_minus_source_alpha,
    .color_operation = sbx::graphics::blend_operation::add,
    .alpha_source = sbx::graphics::blend_factor::source_alpha,
    .alpha_destination = sbx::graphics::blend_factor::one_minus_source_alpha,
    .alpha_operation = sbx::graphics::blend_operation::add,
    .color_write_mask = sbx::graphics::color_component::r | sbx::graphics::color_component::g | sbx::graphics::color_component::b | sbx::graphics::color_component::a
  };

  auto resolve = create_attachment("resolve", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r32g32b32a32_sfloat, resolve_blend);

  auto brightness = create_attachment("brightness", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat);

  auto bloom_0_down = create_attachment("bloom_0_down", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat, sbx::graphics::filter::linear, sbx::graphics::address_mode::clamp_to_edge);
  auto bloom_1_down = create_attachment("bloom_1_down", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat, sbx::graphics::filter::linear, sbx::graphics::address_mode::clamp_to_edge);
  auto bloom_2_down = create_attachment("bloom_2_down", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat, sbx::graphics::filter::linear, sbx::graphics::address_mode::clamp_to_edge);
  auto bloom_2_horizontal_blur = create_attachment("bloom_2_horizontal_blur", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat);
  auto bloom_2_full_blur = create_attachment("bloom_2_full_blur", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat);
  auto bloom_1_up = create_attachment("bloom_1_up", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat);
  auto bloom_0_up = create_attachment("bloom_0_up", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat);

  auto tonemap = create_attachment("tonemap", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_srgb);

  auto fxaa = create_attachment("fxaa", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_srgb);

  auto selection = create_attachment("selection", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_srgb);

  auto swapchain = create_attachment("swapchain", sbx::graphics::attachment::type::swapchain, _clear_color, sbx::graphics::format::b8g8r8a8_srgb);

  // Compute passes
  auto culling_pass = create_pass([&](sbx::graphics::render_graph::context& context) {
    auto pass = context.compute_pass("culling");
    return pass;
  });

  auto skinning_pass = create_pass([&](sbx::graphics::render_graph::context& context) {
    auto pass = context.compute_pass("skinning");
    return pass;
  });

  auto particles_pass = create_pass([&](sbx::graphics::render_graph::context& context) {
    auto pass = context.compute_pass("particles");
    return pass;
  });

  // Render passes
  auto shadow_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("shadow", sbx::graphics::viewport::fixed(2048u, 2048u));

    pass.depends_on(skinning_pass);

    pass.writes(shadow_depth, sbx::graphics::attachment_load_operation::clear);
    pass.writes(shadow, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto gbuffer_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("deferred", scene_viewport);

    pass.depends_on(skinning_pass, culling_pass);

    pass.writes(depth, sbx::graphics::attachment_load_operation::clear);
    pass.writes(albedo, sbx::graphics::attachment_load_operation::clear);
    pass.writes(position, sbx::graphics::attachment_load_operation::clear);
    pass.writes(normal, sbx::graphics::attachment_load_operation::clear);
    pass.writes(material, sbx::graphics::attachment_load_operation::clear);
    pass.writes(emissive, sbx::graphics::attachment_load_operation::clear);
    pass.writes(object_id, sbx::graphics::attachment_load_operation::clear);
    pass.writes(linear_depth, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto transparency_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("transparency", scene_viewport);

    pass.depends_on(gbuffer_pass, culling_pass, skinning_pass, particles_pass);

    pass.writes(depth, sbx::graphics::attachment_load_operation::load);
    pass.writes(accumulator, sbx::graphics::attachment_load_operation::clear);
    pass.writes(revealage, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto resolve_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("resolve", scene_viewport);

    pass.depends_on(gbuffer_pass, transparency_pass, shadow_pass);

    pass.reads(albedo, position, normal, material, emissive, accumulator, revealage, shadow);

    pass.writes(depth, sbx::graphics::attachment_load_operation::load);
    pass.writes(resolve, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto brightness_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("brightness_pass", scene_viewport);

    pass.depends_on(resolve_pass);

    pass.reads(resolve);

    pass.writes(brightness, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  const auto half_viewport    = sbx::graphics::viewport::named("scene", sbx::math::vector2f{0.5f,   0.5f});
  const auto quarter_viewport = sbx::graphics::viewport::named("scene", sbx::math::vector2f{0.25f,  0.25f});
  const auto eighth_viewport  = sbx::graphics::viewport::named("scene", sbx::math::vector2f{0.125f, 0.125f});

  auto bloom_downsample_0_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_downsample_0", half_viewport);

    pass.depends_on(brightness_pass);

    pass.reads(brightness);

    pass.writes(bloom_0_down, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_downsample_1_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_downsample_1", quarter_viewport);

    pass.depends_on(bloom_downsample_0_pass);

    pass.reads(bloom_0_down);

    pass.writes(bloom_1_down, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_downsample_2_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_downsample_2", eighth_viewport);

    pass.depends_on(bloom_downsample_1_pass);

    pass.reads(bloom_1_down);

    pass.writes(bloom_2_down, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_blur_h_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_blur_h", eighth_viewport);

    pass.depends_on(bloom_downsample_2_pass);

    pass.reads(bloom_2_down);

    pass.writes(bloom_2_horizontal_blur, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_blur_v_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_blur_v", eighth_viewport);

    pass.depends_on(bloom_blur_h_pass);

    pass.reads(bloom_2_horizontal_blur);

    pass.writes(bloom_2_full_blur, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_upsample_1_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_upsample_1", quarter_viewport);

    pass.depends_on(bloom_blur_v_pass, bloom_downsample_1_pass);

    pass.reads(bloom_2_full_blur, bloom_1_down);

    pass.writes(bloom_1_up, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_upsample_0_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_upsample_0", half_viewport);

    pass.depends_on(bloom_upsample_1_pass, bloom_downsample_0_pass);

    pass.reads(bloom_1_up, bloom_0_down);

    pass.writes(bloom_0_up, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto tonemap_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("tonemap", scene_viewport);

    pass.depends_on(resolve_pass, bloom_upsample_0_pass);

    pass.reads(resolve, bloom_0_up);

    pass.writes(tonemap, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto fxaa_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("fxaa", scene_viewport);

    pass.depends_on(tonemap_pass);

    pass.reads(tonemap);

    pass.writes(fxaa, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto selection_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("selection", scene_viewport);

    pass.depends_on(fxaa_pass, gbuffer_pass);

    pass.reads(fxaa, object_id, linear_depth);

    pass.writes(selection, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto editor_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("editor");

    pass.depends_on(selection_pass);

    pass.reads(selection);
    pass.writes(swapchain, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  build_render_graph();

  
  // Draw lists
  add_draw_list<sbx::models::static_mesh_material_draw_list>();
  add_draw_list<sbx::animations::skinned_mesh_material_draw_list>();
  
  // Compute passes
  const auto& frustum_culling_task = add_task<sbx::models::frustum_culling_task>(culling_pass);
  const auto& skinning_task = add_task<sbx::animations::skinning_task>(skinning_pass);
  const auto& particle_task = add_task<sbx::particles::particle_task>(particles_pass);

  // Shadow pass
  add_subrenderer<sbx::models::static_mesh_shadow_subrenderer>(shadow_pass);
  add_subrenderer<sbx::animations::skinned_mesh_shadow_subrenderer>(shadow_pass);

  // Deferred pass
  add_subrenderer<sbx::models::static_mesh_material_subrenderer>(gbuffer_pass, sbx::models::static_mesh_material_draw_list::bucket::opaque);
  add_subrenderer<sbx::animations::skinned_mesh_material_subrenderer>(gbuffer_pass, sbx::animations::skinned_mesh_material_draw_list::bucket::opaque);

  // Transparency pass
  add_subrenderer<sbx::models::static_mesh_material_subrenderer>(transparency_pass, sbx::models::static_mesh_material_draw_list::bucket::transparent);
  add_subrenderer<sbx::animations::skinned_mesh_material_subrenderer>(transparency_pass, sbx::animations::skinned_mesh_material_draw_list::bucket::transparent);

  add_subrenderer<sbx::particles::particle_subrenderer>(transparency_pass);

  // Resolve pass
  auto resolve_opaque_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"albedo_image", "albedo"},
    {"position_image", "position"},
    {"normal_image", "normal"},
    {"material_image", "material"},
    {"emissive_image", "emissive"},
    {"shadow_image", "shadow"}
  };

  add_subrenderer<sbx::post::resolve_opaque_filter>(resolve_pass, std::move(resolve_opaque_attachment_names));

  add_subrenderer<sbx::scenes::skybox_subrenderer>(resolve_pass);

  auto resolve_transparent_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"accumulator_image", "accumulator"},
    {"revealage_image", "revealage"}
  };

  add_subrenderer<sbx::post::resolve_transparent_filter>(resolve_pass, std::move(resolve_transparent_attachment_names));

  add_subrenderer<sbx::sprites::sprite_subrenderer>(resolve_pass);

  add_subrenderer<sbx::scenes::grid_subrenderer>(resolve_pass);

  add_subrenderer<sbx::scenes::debug_subrenderer>(resolve_pass);

  // Brightness pass
  add_subrenderer<sbx::post::brightness_filter>(brightness_pass, "resolve", sbx::post::brightness_config{.threshold = 1.0f, .soft_knee = 0.5f});

  // Bloom chain
  add_subrenderer<sbx::post::downsample_filter>(bloom_downsample_0_pass, "engine://shaders/downsample", "brightness");
  add_subrenderer<sbx::post::downsample_filter>(bloom_downsample_1_pass, "engine://shaders/downsample", "bloom_0_down");
  add_subrenderer<sbx::post::downsample_filter>(bloom_downsample_2_pass, "engine://shaders/downsample", "bloom_1_down");

  add_subrenderer<sbx::post::blur_filter_gaussian_9>(bloom_blur_h_pass, "engine://shaders/blur", "bloom_2_down",   sbx::math::vector2{1.0f, 0.0f});
  add_subrenderer<sbx::post::blur_filter_gaussian_9>(bloom_blur_v_pass, "engine://shaders/blur", "bloom_2_horizontal_blur", sbx::math::vector2{0.0f, 1.0f});

  add_subrenderer<sbx::post::upsample_filter>(bloom_upsample_1_pass, "engine://shaders/upsample", "bloom_2_full_blur", "bloom_1_down", 1.0f);
  add_subrenderer<sbx::post::upsample_filter>(bloom_upsample_0_pass, "engine://shaders/upsample", "bloom_1_up", "bloom_0_down", 1.0f);

  // Post-processing pass
  auto tonemap_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"resolve_image", "resolve"},
    {"bloom_image",   "bloom_0_up"}
  };

  auto tonemap_config = sbx::post::tonemap_config{
    .exposure = 0.0f,
    .bloom_mix = 0.8f,
    .saturation = 1.0f,
    .contrast = 1.0f,
    .temperature = 0.0f,
    .tint = 0.0f
  };

  add_subrenderer<sbx::post::tonemap_filter>(tonemap_pass, std::move(tonemap_attachment_names), tonemap_config);

  add_subrenderer<sbx::post::fxaa_filter>(fxaa_pass, "tonemap");

  auto selection_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"resolve_image", "fxaa"},
    {"object_id_image", "object_id"},
    {"linear_depth_image", "linear_depth"}
  };

  add_subrenderer<editor::selection_filter>(selection_pass, "editor://shaders/selection", std::move(selection_attachment_names));

  add_subrenderer<editor::editor_subrenderer>(editor_pass, "selection");
}

} // namespace editor
