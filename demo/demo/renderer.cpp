// SPDX-License-Identifier: MIT
#include <demo/renderer.hpp>

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

#include <libsbx/graphics/pipeline/vertex_input_description.hpp>

#include <libsbx/post/filters/resolve_filter.hpp>
#include <libsbx/post/filters/blur_filter.hpp>
#include <libsbx/post/filters/fxaa_filter.hpp>
#include <libsbx/post/filters/tonemap_filter.hpp>
#include <libsbx/post/filters/downsample_filter.hpp>
#include <libsbx/post/filters/upsample_filter.hpp>
#include <libsbx/post/filters/ssao_filter.hpp>

#include <libsbx/ui/ui_subrenderer.hpp>
#include <libsbx/gizmos/gizmos_subrenderer.hpp>
#include <libsbx/editor/editor_subrenderer.hpp>
#include <libsbx/sprites/sprite_subrenderer.hpp>

#include <demo/application.hpp>
#include <demo/terrain_subrenderer.hpp>

namespace demo {

renderer::renderer()
: _clear_color{sbx::math::color::white()} {
  // Attachments
  auto shadow = create_attachment("shadow", sbx::graphics::attachment::type::image, sbx::math::color::white(), sbx::graphics::format::r32_sfloat, sbx::graphics::filter::linear, sbx::graphics::address_mode::clamp_to_edge);
  auto shadow_depth = create_attachment("shadow_depth", sbx::graphics::attachment::type::depth);

  auto depth = create_attachment("depth", sbx::graphics::attachment::type::depth);
  auto albedo = create_attachment("albedo", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
  auto position = create_attachment("position", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat);
  auto normal = create_attachment("normal", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::a2b10g10r10_unorm_pack32);
  auto material = create_attachment("material", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
  auto emissive = create_attachment("emissive", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
  auto object_id = create_attachment("object_id", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32_uint);
  auto linear_depth = create_attachment("linear_depth", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32_sfloat);

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

  auto accum = create_attachment("accum", sbx::graphics::attachment::type::image, sbx::math::color{0.0f, 0.0f, 0.0f, 0.0f}, sbx::graphics::format::r32g32b32a32_sfloat, accum_blend);
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

  auto bloom_downsample1 = create_attachment("bloom_downsample1", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);
  auto bloom_downsample2 = create_attachment("bloom_downsample2", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);
  auto bloom_blur_horizontal = create_attachment("bloom_blur_horizontal", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);
  auto bloom_blur_vertical = create_attachment("bloom_blur_vertical", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);
  auto bloom_upsample = create_attachment("bloom_upsample", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);

  auto tonemap = create_attachment("tonemap", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
  
  auto fxaa = create_attachment("fxaa", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);

  auto swapchain = create_attachment("swapchain", sbx::graphics::attachment::type::swapchain, _clear_color, sbx::graphics::format::b8g8r8a8_srgb);

  // Compute passes
  auto skinning_pass = create_pass([&](sbx::graphics::render_graph::context& context) {
    auto pass = context.compute_pass("skinning");
    return pass;
  });

  // Render passes
  auto shadow_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("shadow", sbx::graphics::viewport::fixed(2048, 2048));

    pass.depends_on(skinning_pass);

    pass.writes(shadow_depth, sbx::graphics::attachment_load_operation::clear);
    pass.writes(shadow, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto deferred_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("deferred");

    pass.depends_on(skinning_pass);

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
    auto pass = context.graphics_pass("transparency");

    pass.depends_on(deferred_pass);

    pass.writes(depth, sbx::graphics::attachment_load_operation::load);
    pass.writes(accum, sbx::graphics::attachment_load_operation::clear);
    pass.writes(revealage, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });


  auto resolve_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("resolve");

    pass.depends_on(deferred_pass, transparency_pass, shadow_pass);

    pass.reads(albedo, position, normal, material, emissive, accum, revealage, shadow);

    pass.writes(depth, sbx::graphics::attachment_load_operation::load);
    pass.writes(resolve, sbx::graphics::attachment_load_operation::clear);
    pass.writes(brightness, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_downsample1_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_downsample1", sbx::graphics::viewport::window(sbx::math::vector2f{0.5f, 0.5f}));

    pass.depends_on(resolve_pass);

    pass.reads(brightness);

    pass.writes(bloom_downsample1, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_downsample2_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_downsample2", sbx::graphics::viewport::window(sbx::math::vector2f{0.25f, 0.25f}));

    pass.depends_on(bloom_downsample1_pass);

    pass.reads(bloom_downsample1);

    pass.writes(bloom_downsample2, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_blur_horizontal_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_blur_horizontal", sbx::graphics::viewport::window(sbx::math::vector2f{0.25f, 0.25f}));

    pass.depends_on(bloom_downsample2_pass);

    pass.reads(bloom_downsample2);

    pass.writes(bloom_blur_horizontal, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_blur_vertical_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_blur_vertical", sbx::graphics::viewport::window(sbx::math::vector2f{0.25f, 0.25f}));

    pass.depends_on(bloom_blur_horizontal_pass);

    pass.reads(bloom_blur_horizontal);

    pass.writes(bloom_blur_vertical, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto bloom_upsample_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("bloom_upsample", sbx::graphics::viewport::window(sbx::math::vector2f{0.5f, 0.5f}));

    pass.depends_on(bloom_blur_vertical_pass);

    pass.reads(bloom_blur_vertical);
    pass.reads(bloom_downsample1);

    pass.writes(bloom_upsample, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto tonemap_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("tonemap");

    pass.depends_on(resolve_pass);

    pass.reads(resolve);

    pass.writes(tonemap, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto fxaa_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("fxaa");

    pass.depends_on(tonemap_pass);

    pass.reads(tonemap);

    pass.writes(fxaa, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto editor_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("editor");

    pass.depends_on(fxaa_pass);

    pass.reads(fxaa);

    pass.writes(swapchain, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  build_render_graph();

  
  // Draw lists
  add_draw_list<sbx::models::static_mesh_material_draw_list>("static_mesh_material");
  add_draw_list<sbx::animations::skinned_mesh_material_draw_list>("skinned_mesh_material");
  
  auto& skinning = add_task<sbx::animations::skinning_task>(skinning_pass, "res://shaders/skinning");

  // Shadow pass
  add_subrenderer<sbx::models::static_mesh_shadow_subrenderer>(shadow_pass, "res://shaders/shadow");
  add_subrenderer<sbx::animations::skinned_mesh_shadow_subrenderer>(shadow_pass, "res://shaders/shadow", skinning.vertex_buffer_handle());

  // Deferred pass
  add_subrenderer<sbx::models::static_mesh_material_subrenderer>(deferred_pass, "res://shaders/deferred_pbr_material", sbx::models::static_mesh_material_draw_list::bucket::opaque);
  add_subrenderer<sbx::animations::skinned_mesh_material_subrenderer>(deferred_pass, "res://shaders/deferred_pbr_material", sbx::animations::skinned_mesh_material_draw_list::bucket::opaque, skinning.vertex_buffer_handle());

  // _terrain_subrenderer = sbx::memory::make_observer(add_subrenderer<terrain_subrenderer>(deferred_pass, "res://shaders/terrain"));

  // Transparency pass
  add_subrenderer<sbx::models::static_mesh_material_subrenderer>(transparency_pass, "res://shaders/deferred_pbr_material", sbx::models::static_mesh_material_draw_list::bucket::transparent);
  add_subrenderer<sbx::animations::skinned_mesh_material_subrenderer>(transparency_pass, "res://shaders/deferred_pbr_material", sbx::animations::skinned_mesh_material_draw_list::bucket::transparent, skinning.vertex_buffer_handle());

  // Resolve pass
  auto resolve_opaque_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"albedo_image", "albedo"},
    {"position_image", "position"},
    {"normal_image", "normal"},
    {"material_image", "material"},
    {"emissive_image", "emissive"},
    // {"ssao_image", "ssao"}.
    {"shadow_image", "shadow"},
  };

  add_subrenderer<sbx::post::resolve_opaque_filter>(resolve_pass, "res://shaders/resolve_opaque", std::move(resolve_opaque_attachment_names));

  add_subrenderer<sbx::scenes::skybox_subrenderer>(resolve_pass, "res://shaders/skybox");

  auto resolve_transparent_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"accum_image", "accum"},
    {"revealage_image", "revealage"}
  };

  add_subrenderer<sbx::post::resolve_transparent_filter>(resolve_pass, "res://shaders/resolve_transparent", std::move(resolve_transparent_attachment_names));

  add_subrenderer<sbx::sprites::sprite_subrenderer>(resolve_pass, "res://shaders/sprites");

  add_subrenderer<sbx::scenes::grid_subrenderer>(resolve_pass, "res://shaders/grid");

  add_subrenderer<sbx::scenes::debug_subrenderer>(resolve_pass, "res://shaders/debug");

  // Post-processing pass
  add_subrenderer<sbx::post::downsample_filter>(bloom_downsample1_pass, "res://shaders/downsample", "brightness");
  add_subrenderer<sbx::post::downsample_filter>(bloom_downsample2_pass, "res://shaders/downsample", "bloom_downsample1");
  
  add_subrenderer<sbx::post::blur_filter_gaussian_13>(bloom_blur_horizontal_pass, "res://shaders/blur", "bloom_downsample2", sbx::math::vector2f{1.0f, 0.0f});
  add_subrenderer<sbx::post::blur_filter_gaussian_13>(bloom_blur_vertical_pass, "res://shaders/blur", "bloom_blur_horizontal", sbx::math::vector2f{0.0f, 1.0f});

  add_subrenderer<sbx::post::upsample_filter>(bloom_upsample_pass, "res://shaders/upsample", "bloom_blur_vertical", "bloom_downsample1");

  auto tonemap_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"resolve_image", "resolve"},
    {"bloom_image", "bloom_upsample"}
  };

  auto tonemap_config = sbx::post::tonemap_config{
    .exposure = 0.0f,
    .bloom_mix = 0.8f,
    .saturation = 1.0f,
    .contrast = 1.0f,
    .temperature = 0.0f,
    .tint = 0.0f
  };

  add_subrenderer<sbx::post::tonemap_filter>(tonemap_pass, "res://shaders/tonemap", std::move(tonemap_attachment_names), tonemap_config);

  add_subrenderer<sbx::post::fxaa_filter>(fxaa_pass, "res://shaders/fxaa", "tonemap");

  // Editor pass
  add_subrenderer<sbx::editor::editor_subrenderer>(editor_pass, "res://shaders/editor", "fxaa");
}

} // namespace demo
