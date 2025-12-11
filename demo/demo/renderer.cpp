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
  auto [
    deferred,
    transparency,
    resolve,
    post, 
    editor
  ] = create_graph(
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto deferred_pass = context.graphics_pass("deferred");

      deferred_pass.produces("depth", sbx::graphics::attachment::type::depth);
      deferred_pass.produces("albedo", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
      deferred_pass.produces("position", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r16g16b16a16_sfloat);
      deferred_pass.produces("normal", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::a2b10g10r10_unorm_pack32);
      deferred_pass.produces("material", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
      deferred_pass.produces("emissive", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
      deferred_pass.produces("object_id", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32_uint);
      deferred_pass.produces("linear_depth", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32_sfloat);

      return deferred_pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto transparency_pass = context.graphics_pass("transparency");

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

      transparency_pass.produces("depth", sbx::graphics::attachment::type::depth);
      transparency_pass.produces("accum", sbx::graphics::attachment::type::image, sbx::math::color{0.0f, 0.0f, 0.0f, 0.0f}, sbx::graphics::format::r32g32b32a32_sfloat, accum_blend);
      transparency_pass.produces("revealage", sbx::graphics::attachment::type::image, sbx::math::color{1.0f, 0.0f, 0.0f, 0.0f}, sbx::graphics::format::r32_sfloat, revealage_blend);

      return transparency_pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto resolve_pass = context.graphics_pass("resolve");

      const auto resolve_blend = sbx::graphics::blend_state{
        .color_source = sbx::graphics::blend_factor::source_alpha,
        .color_destination = sbx::graphics::blend_factor::one_minus_source_alpha,
        .color_operation = sbx::graphics::blend_operation::add,
        .alpha_source = sbx::graphics::blend_factor::source_alpha,
        .alpha_destination = sbx::graphics::blend_factor::one_minus_source_alpha,
        .alpha_operation = sbx::graphics::blend_operation::add,
        .color_write_mask = sbx::graphics::color_component::r | sbx::graphics::color_component::g | sbx::graphics::color_component::b | sbx::graphics::color_component::a
      };

      resolve_pass.uses("albedo", "position", "normal", "material", "emissive", "object_id", "accum", "revealage");

      resolve_pass.produces("depth", sbx::graphics::attachment::type::depth);
      resolve_pass.produces("resolve", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r32g32b32a32_sfloat, resolve_blend);

      return resolve_pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto post_pass = context.graphics_pass("post");

      post_pass.uses("resolve");

      post_pass.produces("post", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);

      return post_pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto editor_pass = context.graphics_pass("editor");

      editor_pass.uses("post");

      editor_pass.produces("swapchain", sbx::graphics::attachment::type::swapchain, _clear_color, sbx::graphics::format::b8g8r8a8_srgb);

      return editor_pass;
    }
  );

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
