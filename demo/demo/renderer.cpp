#include <demo/renderer.hpp>

#include <libsbx/core/engine.hpp>


#include <libsbx/scenes/skybox_subrenderer.hpp>
#include <libsbx/scenes/debug_subrenderer.hpp>
#include <libsbx/scenes/grid_subrenderer.hpp>

#include <libsbx/models/models.hpp>
#include <libsbx/models/frustum_culling_task.hpp>
#include <libsbx/models/foliage_task.hpp>
#include <libsbx/models/foliage_subrenderer.hpp>
#include <libsbx/models/static_mesh_subrenderer.hpp>

#include <libsbx/animations/skinned_mesh_subrenderer.hpp>

#include <libsbx/graphics/pipeline/vertex_input_description.hpp>

#include <libsbx/post/filters/resolve_filter.hpp>
#include <libsbx/post/filters/blur_filter.hpp>
#include <libsbx/post/filters/fxaa_filter.hpp>

#include <libsbx/shadows/shadow_subrenderer.hpp>
#include <libsbx/ui/ui_subrenderer.hpp>
#include <libsbx/gizmos/gizmos_subrenderer.hpp>
#include <libsbx/editor/editor_subrenderer.hpp>

#include <demo/terrain/terrain_subrenderer.hpp>
#include <demo/terrain/planet_generator_task.hpp>

namespace demo {

renderer::renderer()
: _clear_color{sbx::math::color::white()} {

  auto depth = create_attachment("depth", sbx::graphics::attachment::type::depth);
  auto albedo = create_attachment("albedo", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
  auto position = create_attachment("position", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat);
  auto normal = create_attachment("denormalpth", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::a2b10g10r10_unorm_pack32);
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

  auto fxaa = create_attachment("fxaa", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);

  auto swapchain = create_attachment("swapchain", sbx::graphics::attachment::type::swapchain, _clear_color, sbx::graphics::format::b8g8r8a8_srgb);

  auto deferred = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("deferred");

    pass.writes(depth, sbx::graphics::attachment_load_operation::clear);
    pass.writes(albedo, sbx::graphics::attachment_load_operation::clear);
    pass.writes(position, sbx::graphics::attachment_load_operation::clear);
    pass.writes(normal, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto transparency = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("transparency");

    pass.depends_on(deferred);

    pass.writes(depth, sbx::graphics::attachment_load_operation::load);
    pass.writes(accum, sbx::graphics::attachment_load_operation::clear);
    pass.writes(revealage, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto resolve = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("deferred");

    pass.depends_on(deferred, transparency);

    pass.reads(albedo, position, normal, material, emissive, object_id, accum, revealage);

    pass.writes(resolve, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto fxaa = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("fxaa");

    pass.depends_on(resolve);

    pass.reads(resolve);

    pass.writes(fxaa, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto editor = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("editor");

    pass.depends_on(fxaa);

    pass.reads(fxaa);

    pass.writes(swapchain, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  // Draw lists
  add_draw_list<sbx::models::static_mesh_material_draw_list>("static_mesh_material");
  add_draw_list<sbx::animations::skinned_mesh_material_draw_list>("skinned_mesh_material");

  // Shadow pass
  // add_subrenderer<sbx::shadows::shadow_subrenderer>(shadow, "res://shaders/shadow");

  // Deferred pass
  add_subrenderer<sbx::models::static_mesh_subrenderer>(deferred, "res://shaders/deferred_pbr_material", sbx::models::static_mesh_material_draw_list::bucket::opaque);
  add_subrenderer<sbx::animations::skinned_mesh_subrenderer>(deferred, "res://shaders/deferred_pbr_material", sbx::animations::skinned_mesh_material_draw_list::bucket::opaque);

  // Transparency pass
  add_subrenderer<sbx::models::static_mesh_subrenderer>(transparency, "res://shaders/deferred_pbr_material", sbx::models::static_mesh_material_draw_list::bucket::transparent);
  add_subrenderer<sbx::animations::skinned_mesh_subrenderer>(transparency, "res://shaders/deferred_pbr_material", sbx::animations::skinned_mesh_material_draw_list::bucket::transparent);

  // Resolve pass
  auto resolve_opaque_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"albedo_image", "albedo"},
    {"position_image", "position"},
    {"normal_image", "normal"},
    {"material_image", "material"},
    {"emissive_image", "emissive"},
    // {"shadow_image", "shadow"},
    // {"object_id_image", "object_id"}
  };

  add_subrenderer<sbx::post::resolve_opaque_filter>(resolve, "res://shaders/resolve_opaque", std::move(resolve_opaque_attachment_names));

  add_subrenderer<sbx::scenes::skybox_subrenderer>(resolve, "res://shaders/skybox");

  auto resolve_transparent_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"accum_image", "accum"},
    {"revealage_image", "revealage"}
  };

  add_subrenderer<sbx::post::resolve_transparent_filter>(resolve, "res://shaders/resolve_transparent", std::move(resolve_transparent_attachment_names));

  add_subrenderer<sbx::scenes::grid_subrenderer>(resolve, "res://shaders/grid");

  add_subrenderer<sbx::scenes::debug_subrenderer>(resolve, "res://shaders/debug");

  // Post-processing pass
  add_subrenderer<sbx::post::fxaa_filter>(post, "res://shaders/fxaa", "resolve");

  // Editor pass
  add_subrenderer<sbx::editor::editor_subrenderer>(editor, "res://shaders/editor", "post");
}

} // namespace demo
