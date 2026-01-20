#include <demo/renderer.hpp>

#include <libsbx/assets/assets_module.hpp>

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
#include <libsbx/post/filters/tonemap_filter.hpp>
#include <libsbx/post/filters/downsample_filter.hpp>
#include <libsbx/post/filters/upsample_filter.hpp>
#include <libsbx/post/filters/ssao_filter.hpp>

#include <libsbx/shadows/shadow_subrenderer.hpp>
#include <libsbx/ui/ui_subrenderer.hpp>
#include <libsbx/gizmos/gizmos_subrenderer.hpp>
#include <libsbx/editor/editor_subrenderer.hpp>
#include <libsbx/sprites/sprite_subrenderer.hpp>

#include <demo/application.hpp>

namespace demo {

class terrain_subrenderer : public sbx::graphics::subrenderer {

  class pipeline : public sbx::graphics::graphics_pipeline {

    inline static const auto pipeline_definition = sbx::graphics::pipeline_definition{
      .depth = sbx::graphics::depth::read_write,
      .uses_transparency = true,
      .rasterization_state = sbx::graphics::rasterization_state{
        .polygon_mode = sbx::graphics::polygon_mode::fill,
        .cull_mode = sbx::graphics::cull_mode::none,
        .front_face = sbx::graphics::front_face::counter_clockwise
      }
    };

    using base = sbx::graphics::graphics_pipeline;
  public:

    pipeline(const std::filesystem::path& path, const std::vector<sbx::graphics::attachment_description>& attachments)
    : base{path, attachments, pipeline_definition} { }

    ~pipeline() override = default;

  }; // class pipeline

  inline static auto _is_grid_data_initialized = false;

public:

  struct transform_data {
    sbx::math::matrix4x4 model;
    sbx::math::matrix4x4 normal;
  }; // struct transform_data

  struct instance_data {
    std::uint32_t transform_index;
    std::uint32_t grid_quad_index;
    std::float_t height;
    std::float_t red;
  
    std::float_t green;
    std::float_t blue;
    std::uint32_t padding0;
    std::uint32_t padding1;
  }; // struct instance_data

  terrain_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path)
  : sbx::graphics::subrenderer{},
    _pipeline{path, attachments},
    _push_handler{_pipeline},
    _descriptor_handler{_pipeline, 0u} {
    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

    _grid_vertex_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    _grid_quad_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    _transform_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    _instance_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  }

  ~terrain_subrenderer() override = default;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override {
    SBX_PROFILE_SCOPE("terrain_subrenderer::render");

    auto& application = sbx::core::engine::get_application<demo::application>();
    auto& grid = application.grid();

    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

    auto& grid_vertex_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_grid_vertex_buffer);
    auto& grid_quad_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_grid_quad_buffer);
    auto& transform_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_transform_buffer);
    auto& instance_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_instance_buffer);

    if (!_is_grid_data_initialized) {
      _update_buffer(grid_vertex_buffer, grid.vertices());
      _update_buffer(grid_quad_buffer, grid.quads());

      _is_grid_data_initialized = true;
    }

    auto instances_per_mesh = std::unordered_map<sbx::math::uuid, std::vector<instance_data>>{};
    auto transforms = std::vector<transform_data>{};

    auto terrain_query = scene.query<const terrain_tag>();

    for (auto [node, tag] : terrain_query.each()) {
      const auto& mesh_id = tag.mesh_id;

      const auto transform_index = static_cast<std::uint32_t>(transforms.size());

      // transforms.push_back(transform_data{scene.world_transform(node), scene.world_normal(node)});
      transforms.push_back(transform_data{sbx::math::matrix_cast<4, 4>(scene.world_rotation(node)), scene.world_normal(node)});

      auto& instances = instances_per_mesh[mesh_id];

      const auto quad_index = static_cast<std::uint32_t>(tag.grid_cell);

      instances.push_back(instance_data{transform_index, quad_index, tag.height, tag.color.r(), tag.color.g(), tag.color.b(), 0u, 0u});
    }

    _update_buffer(transform_buffer, transforms);

    _pipeline.bind(command_buffer);

    _descriptor_handler.push("scene", scene.uniform_handler());

    _push_handler.push("grid_vertex_data_buffer", grid_vertex_buffer.address());
    _push_handler.push("grid_quad_data_buffer", grid_quad_buffer.address());
    _push_handler.push("transform_data_buffer", transform_buffer.address());

    if (!_descriptor_handler.update(_pipeline)) {
      return;
    }

    _descriptor_handler.bind_descriptors(command_buffer);

    for (const auto& [mesh_id, instances] : instances_per_mesh) {
      auto& mesh = assets_module.get_asset<sbx::models::mesh>(mesh_id);

      _update_buffer(instance_buffer, instances);

      _push_handler.push("instance_data_buffer", instance_buffer.address());

      mesh.bind(command_buffer);

      _push_handler.push("vertex_buffer", mesh.address());

      _push_handler.bind(command_buffer);

      command_buffer.draw_indexed(mesh.index_count(), static_cast<std::uint32_t>(instances.size()), 0u, 0u, 0u);
    }
  }

private:

  template<typename Type>
  auto _update_buffer(sbx::graphics::storage_buffer& buffer, const std::vector<Type>& data) -> void {
    const auto required_size = static_cast<VkDeviceSize>(data.size() * sizeof(Type));

    if (required_size > buffer.size()) {
      buffer.resize(static_cast<VkDeviceSize>(required_size * 1.5f));
    }

    buffer.update(data.data(), data.size() * sizeof(Type));
  }

  pipeline _pipeline;

  sbx::graphics::push_handler _push_handler;
  sbx::graphics::descriptor_handler _descriptor_handler;

  sbx::graphics::storage_buffer_handle _grid_vertex_buffer;
  sbx::graphics::storage_buffer_handle _grid_quad_buffer;
  sbx::graphics::storage_buffer_handle _transform_buffer;
  sbx::graphics::storage_buffer_handle _instance_buffer;

}; // class terrain_subrenderer

renderer::renderer()
: _clear_color{sbx::math::color::white()} {
  // Attachments
  auto shadow = create_attachment("shadow", sbx::graphics::attachment::type::image, sbx::math::color::white(), sbx::graphics::format::r16g16_sfloat);

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

  // auto ssao = create_attachment("ssao", sbx::graphics::attachment::type::image, sbx::math::color::white(), sbx::graphics::format::r32_sfloat);

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
  
  // auto bloom_downsample1 = create_attachment("bloom_downsample1", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);
  // auto bloom_downsample2 = create_attachment("bloom_downsample2", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);
  // auto bloom_blur_horizontal = create_attachment("bloom_blur_horizontal", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);
  // auto bloom_blur_vertical = create_attachment("bloom_blur_vertical", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);
  // auto bloom_upsample = create_attachment("bloom_upsample", sbx::graphics::attachment::type::image, sbx::math::color::black(), sbx::graphics::format::r32g32b32a32_sfloat);

  auto tonemap = create_attachment("tonemap", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);
  
  auto fxaa = create_attachment("fxaa", sbx::graphics::attachment::type::image, _clear_color, sbx::graphics::format::r8g8b8a8_unorm);

  auto swapchain = create_attachment("swapchain", sbx::graphics::attachment::type::swapchain, _clear_color, sbx::graphics::format::b8g8r8a8_srgb);

  // Render passes
  auto shadow_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("shadow", sbx::graphics::viewport::fixed(2048, 2048));

    pass.writes(shadow, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  auto deferred_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("deferred");

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

  // auto ssao_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
  //   auto pass = context.graphics_pass("ssao");

  //   pass.depends_on(deferred_pass);

  //   pass.reads(position, normal);
  //   pass.writes(ssao, sbx::graphics::attachment_load_operation::clear);

  //   return pass;
  // });

  auto resolve_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("resolve");

    pass.depends_on(deferred_pass, transparency_pass, shadow_pass);

    pass.reads(albedo, position, normal, material, emissive, accum, revealage, shadow);

    pass.writes(depth, sbx::graphics::attachment_load_operation::load);
    pass.writes(resolve, sbx::graphics::attachment_load_operation::clear);
    pass.writes(brightness, sbx::graphics::attachment_load_operation::clear);

    return pass;
  });

  // auto bloom_downsample1_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
  //   auto pass = context.graphics_pass("bloom_downsample1", sbx::graphics::viewport::window(sbx::math::vector2f{0.5f, 0.5f}));

  //   pass.depends_on(resolve_pass);

  //   pass.reads(brightness);

  //   pass.writes(bloom_downsample1, sbx::graphics::attachment_load_operation::clear);

  //   return pass;
  // });

  // auto bloom_downsample2_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
  //   auto pass = context.graphics_pass("bloom_downsample2", sbx::graphics::viewport::window(sbx::math::vector2f{0.25f, 0.25f}));

  //   pass.depends_on(bloom_downsample1_pass);

  //   pass.reads(bloom_downsample1);

  //   pass.writes(bloom_downsample2, sbx::graphics::attachment_load_operation::clear);

  //   return pass;
  // });

  // auto bloom_blur_horizontal_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
  //   auto pass = context.graphics_pass("bloom_blur_horizontal", sbx::graphics::viewport::window(sbx::math::vector2f{0.25f, 0.25f}));

  //   pass.depends_on(bloom_downsample2_pass);

  //   pass.reads(bloom_downsample2);

  //   pass.writes(bloom_blur_horizontal, sbx::graphics::attachment_load_operation::clear);

  //   return pass;
  // });

  // auto bloom_blur_vertical_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
  //   auto pass = context.graphics_pass("bloom_blur_vertical", sbx::graphics::viewport::window(sbx::math::vector2f{0.25f, 0.25f}));

  //   pass.depends_on(bloom_blur_horizontal_pass);

  //   pass.reads(bloom_blur_horizontal);

  //   pass.writes(bloom_blur_vertical, sbx::graphics::attachment_load_operation::clear);

  //   return pass;
  // });

  // auto bloom_upsample_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
  //   auto pass = context.graphics_pass("bloom_upsample", sbx::graphics::viewport::window(sbx::math::vector2f{0.5f, 0.5f}));

  //   pass.depends_on(bloom_blur_vertical_pass);

  //   pass.reads(bloom_blur_vertical);
  //   pass.reads(bloom_downsample1);

  //   pass.writes(bloom_upsample, sbx::graphics::attachment_load_operation::clear);

  //   return pass;
  // });

  auto tonemap_pass = create_pass([&](sbx::graphics::render_graph::context& context) -> sbx::graphics::pass_node {
    auto pass = context.graphics_pass("tonemap");

    pass.depends_on(resolve_pass);
    // pass.depends_on(bloom_upsample_pass);

    pass.reads(resolve);
    // pass.reads(bloom_upsample);

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

  // Shadow pass
  // add_subrenderer<sbx::shadows::shadow_subrenderer>(shadow, "res://shaders/shadow");

  // Deferred pass
  add_subrenderer<sbx::models::static_mesh_subrenderer>(deferred_pass, "res://shaders/deferred_pbr_material", sbx::models::static_mesh_material_draw_list::bucket::opaque);
  add_subrenderer<sbx::animations::skinned_mesh_subrenderer>(deferred_pass, "res://shaders/deferred_pbr_material", sbx::animations::skinned_mesh_material_draw_list::bucket::opaque);

  add_subrenderer<terrain_subrenderer>(deferred_pass, "res://shaders/terrain");

  // auto ssao_attachment_names = std::vector<std::pair<std::string, std::string>>{
  //   {"position_image", "position"},
  //   {"normal_image", "normal"}
  // };

  // add_subrenderer<sbx::post::ssao_filter>(ssao_pass, "res://shaders/ssao", std::move(ssao_attachment_names));

  // Transparency pass
  add_subrenderer<sbx::models::static_mesh_subrenderer>(transparency_pass, "res://shaders/deferred_pbr_material", sbx::models::static_mesh_material_draw_list::bucket::transparent);
  add_subrenderer<sbx::animations::skinned_mesh_subrenderer>(transparency_pass, "res://shaders/deferred_pbr_material", sbx::animations::skinned_mesh_material_draw_list::bucket::transparent);

  // Resolve pass
  auto resolve_opaque_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"albedo_image", "albedo"},
    {"position_image", "position"},
    {"normal_image", "normal"},
    {"material_image", "material"},
    {"emissive_image", "emissive"},
    // {"ssao_image", "ssao"}.
    // {"shadow_image", "shadow"},
  };

  add_subrenderer<sbx::post::resolve_opaque_filter>(resolve_pass, "res://shaders/resolve_opaque", std::move(resolve_opaque_attachment_names));

  add_subrenderer<sbx::scenes::skybox_subrenderer>(resolve_pass, "res://shaders/skybox");

  auto resolve_transparent_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"accum_image", "accum"},
    {"revealage_image", "revealage"}
  };

  add_subrenderer<sbx::post::resolve_transparent_filter>(resolve_pass, "res://shaders/resolve_transparent", std::move(resolve_transparent_attachment_names));

  // add_subrenderer<sbx::sprites::sprite_subrenderer>(resolve_pass, "res://shaders/sprites");

  // add_subrenderer<sbx::scenes::grid_subrenderer>(resolve_pass, "res://shaders/grid");

  add_subrenderer<sbx::scenes::debug_subrenderer>(resolve_pass, "res://shaders/debug");

  // Post-processing pass
  // add_subrenderer<sbx::post::downsample_filter>(bloom_downsample1_pass, "res://shaders/downsample", "brightness");
  // add_subrenderer<sbx::post::downsample_filter>(bloom_downsample2_pass, "res://shaders/downsample", "bloom_downsample1");
  
  // add_subrenderer<sbx::post::blur_filter_gaussian_13>(bloom_blur_horizontal_pass, "res://shaders/blur", "bloom_downsample2", sbx::math::vector2f{1.0f, 0.0f});
  // add_subrenderer<sbx::post::blur_filter_gaussian_13>(bloom_blur_vertical_pass, "res://shaders/blur", "bloom_blur_horizontal", sbx::math::vector2f{0.0f, 1.0f});

  // add_subrenderer<sbx::post::upsample_filter>(bloom_upsample_pass, "res://shaders/upsample", "bloom_blur_vertical", "bloom_downsample1");

  auto tonemap_attachment_names = std::vector<std::pair<std::string, std::string>>{
    {"resolve_image", "resolve"},
    // {"bloom_image", "bloom_upsample"}
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
