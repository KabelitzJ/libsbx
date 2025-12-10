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
#include <libsbx/post/filters/selection_filter.hpp>
#include <libsbx/post/filters/tonemap_filter.hpp>
#include <libsbx/post/filters/bloom_filter.hpp>
#include <libsbx/post/filters/downsample_filter.hpp>
#include <libsbx/post/filters/upsample_filter.hpp>
#include <libsbx/post/filters/ssao_filter.hpp>

#include <libsbx/shadows/shadow_subrenderer.hpp>
#include <libsbx/ui/ui_subrenderer.hpp>
#include <libsbx/gizmos/gizmos_subrenderer.hpp>
#include <libsbx/editor/editor_subrenderer.hpp>

#include <demo/terrain/terrain_subrenderer.hpp>
#include <demo/terrain/planet_generator_task.hpp>

namespace demo {

renderer::renderer()
: _clear_color{sbx::math::color::white()} {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto [
    deferred,
    transparency, 
    resolve,
    downsample_1, 
    downsample_2, 
    bloom_x, 
    bloom_y,
    upsample, 
    tonemap, 
    fxaa, 
    selection, 
    editor
  ] = create_graph(
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("deferred");

      pass.produces("depth", sbx::graphics::attachment::type::depth);
      pass.produces("albedo", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
      pass.produces("position", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r16g16b16a16_sfloat);
      pass.produces("normal", sbx::graphics::attachment::type::image, sbx::math::color{0.5f, 0.5f, 1.0f, 1.0f}, sbx::graphics::format::a2b10g10r10_unorm_pack32);
      pass.produces("material", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
      pass.produces("emissive", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r16g16b16a16_sfloat);
      pass.produces("object_id", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32_uint);
      pass.produces("linear_depth", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r32_sfloat);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("transparency");

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

      pass.produces("depth", sbx::graphics::attachment::type::depth);
      pass.produces("accum", sbx::graphics::attachment::type::image, sbx::math::color{0.0f, 0.0f, 0.0f, 0.0f}, sbx::graphics::format::r32g32b32a32_sfloat, accum_blend);
      pass.produces("revealage", sbx::graphics::attachment::type::image, sbx::math::color{1.0f, 0.0f, 0.0f, 0.0f}, sbx::graphics::format::r32_sfloat, revealage_blend);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("resolve");

      const auto resolve_blend = sbx::graphics::blend_state{
        .color_source = sbx::graphics::blend_factor::source_alpha,
        .color_destination = sbx::graphics::blend_factor::one_minus_source_alpha,
        .color_operation = sbx::graphics::blend_operation::add,
        .alpha_source = sbx::graphics::blend_factor::source_alpha,
        .alpha_destination = sbx::graphics::blend_factor::one_minus_source_alpha,
        .alpha_operation = sbx::graphics::blend_operation::add,
        .color_write_mask = sbx::graphics::color_component::r | sbx::graphics::color_component::g | sbx::graphics::color_component::b | sbx::graphics::color_component::a
      };

      pass.uses("albedo", "position", "normal", "material", "emissive", "object_id", "accum", "revealage");

      pass.produces("depth", sbx::graphics::attachment::type::depth);
      pass.produces("resolve", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r32g32b32a32_sfloat, resolve_blend);
      pass.produces("brightness", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("downsample_1", sbx::graphics::viewport::window(sbx::math::vector2f{0.5f, 0.5f}));

      pass.uses("brightness");

      pass.produces("downsample_1", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("downsample_2", sbx::graphics::viewport::window(sbx::math::vector2f{0.25f, 0.25f}));

      pass.uses("downsample_1");

      pass.produces("downsample_2", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("bloom_x", sbx::graphics::viewport::window(sbx::math::vector2f{0.125f, 0.125f}));

      pass.uses("downsample_2");

      pass.produces("bloom_x", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("bloom_full", sbx::graphics::viewport::window(sbx::math::vector2f{0.125f, 0.125f}));

      pass.uses("bloom_x");

      pass.produces("bloom_full", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("upsample", sbx::graphics::viewport::window(sbx::math::vector2f{0.25f, 0.25f}));

      pass.uses("bloom_full");
      pass.uses("downsample_1");

      pass.produces("upsample", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("tonemap");

      pass.uses("resolve");
      pass.uses("upsample");

      pass.produces("tonemap", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("fxaa");

      pass.uses("tonemap");
      pass.produces("fxaa", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("selection");

      pass.uses("fxaa");
      pass.uses("object_id");
      pass.uses("linear_depth");

      pass.produces("selection", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);

      return pass;
    },
    [&](sbx::graphics::render_graph::context& context) -> sbx::graphics::render_graph::graphics_pass {
      auto pass = context.graphics_pass("editor");

      pass.uses("selection");
      pass.produces("swapchain", sbx::graphics::attachment::type::swapchain, _clear_color, sbx::graphics::format::b8g8r8a8_srgb);

      return pass;
    }
  );

  // draw lists
  add_draw_list<sbx::models::static_mesh_material_draw_list>("static_mesh_material");
  add_draw_list<sbx::animations::skinned_mesh_material_draw_list>("skinned_mesh_material");

  // deferred pass
  add_subrenderer<sbx::models::static_mesh_subrenderer>(deferred, "res://shaders/deferred_pbr_material", sbx::models::static_mesh_material_draw_list::bucket::opaque);
  add_subrenderer<sbx::animations::skinned_mesh_subrenderer>(deferred, "res://shaders/deferred_pbr_material", sbx::animations::skinned_mesh_material_draw_list::bucket::opaque);

  // transparency pass
  add_subrenderer<sbx::models::static_mesh_subrenderer>(transparency, "res://shaders/deferred_pbr_material", sbx::models::static_mesh_material_draw_list::bucket::transparent);
  add_subrenderer<sbx::animations::skinned_mesh_subrenderer>(transparency, "res://shaders/deferred_pbr_material", sbx::animations::skinned_mesh_material_draw_list::bucket::transparent);

  // resolve pass
  auto resolve_opaque_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"albedo_image", "albedo"},
    {"position_image", "position"},
    {"normal_image", "normal"},
    {"material_image", "material"},
    {"emissive_image", "emissive"}
  };

  add_subrenderer<sbx::post::resolve_opaque_filter>(resolve, "res://shaders/resolve_opaque", std::move(resolve_opaque_attachment_names));
  
  auto resolve_transparent_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"accum_image", "accum"},
    {"revealage_image", "revealage"}
  };
  
  add_subrenderer<sbx::post::resolve_transparent_filter>(resolve, "res://shaders/resolve_transparent", std::move(resolve_transparent_attachment_names));
  add_subrenderer<sbx::scenes::skybox_subrenderer>(resolve, "res://shaders/skybox");
  // add_subrenderer<sbx::scenes::grid_subrenderer>(resolve, "res://shaders/grid");
  // add_subrenderer<sbx::scenes::debug_subrenderer>(resolve, "res://shaders/debug");

  // post-processing passes
  add_subrenderer<sbx::post::downsample_filter>(downsample_1, "res://shaders/downsample", "brightness");
  add_subrenderer<sbx::post::downsample_filter>(downsample_2, "res://shaders/downsample", "downsample_1");

  add_subrenderer<sbx::post::blur_filter_gaussian_13>(bloom_x, "res://shaders/blur", "downsample_2", sbx::math::vector2{1.0f, 0.0f});
  add_subrenderer<sbx::post::blur_filter_gaussian_13>(bloom_y, "res://shaders/blur", "bloom_x", sbx::math::vector2{0.0f, 1.0f});

  add_subrenderer<sbx::post::upsample_filter>(upsample, "res://shaders/upsample", "bloom_full", "downsample_1", 1.0f);

  auto tonemap_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"resolve_image", "resolve"},
    {"bloom_image", "upsample"}
  };

  add_subrenderer<sbx::post::tonemap_filter>(tonemap, "res://shaders/tonemap", std::move(tonemap_attachment_names));

  add_subrenderer<sbx::post::fxaa_filter>(fxaa, "res://shaders/fxaa", "resolve");

  auto selection_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"resolve_image", "fxaa"},
    {"object_id_image", "object_id"},
    {"linear_depth_image", "linear_depth"},
  };

  add_subrenderer<sbx::post::selection_filter>(selection, "res://shaders/selection", std::move(selection_attachment_names));

  add_subrenderer<sbx::editor::editor_subrenderer>(editor, "res://shaders/editor", "selection");
}

} // namespace demo
