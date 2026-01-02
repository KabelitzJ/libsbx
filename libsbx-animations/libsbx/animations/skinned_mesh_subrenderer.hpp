#ifndef LIBSBX_ANIMATIONS_SKINNED_MESH_SUBRENDERER_HPP_
#define LIBSBX_ANIMATIONS_SKINNED_MESH_SUBRENDERER_HPP_

#include <filesystem>
#include <unordered_set>
#include <ranges>
#include <algorithm>
#include <iterator>
#include <ranges>
#include <algorithm>

#include <easy/profiler.h>

#include <fmt/format.h>

#include <range/v3/view/enumerate.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/memory/monotonic_arena.hpp>

#include <libsbx/containers/octree.hpp>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/layout.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/profiler.hpp>

#include <libsbx/devices/input.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>
#include <libsbx/graphics/buffers/uniform_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>
#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>

#include <libsbx/scenes/components/skinned_mesh.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/point_light.hpp>
#include <libsbx/scenes/components/global_transform.hpp>

#include <libsbx/models/material_draw_list.hpp>
#include <libsbx/models/vertex3d.hpp>

#include <libsbx/animations/vertex3d.hpp>
#include <libsbx/animations/mesh.hpp>
#include <libsbx/animations/animator.hpp>

namespace sbx::animations {

struct skinned_mesh_traits {

  using mesh_type = animations::mesh;
  struct instance_payload { };

  struct skinning_job {
    graphics::buffer::address_type pre_vertices;
    graphics::buffer::address_type post_vertices;
    std::uint32_t vertex_count;
    std::uint32_t bone_offset;
  }; // struct skinning_job

  inline static const auto bone_matrices_buffer_name = utility::hashed_string{"bone_matrices"};

  template<typename DrawList>
  static auto create_shared_buffers(DrawList& draw_list) -> void {
    draw_list.create_buffer(bone_matrices_buffer_name, graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  }

  template<typename DrawList>
  static auto destroy_shared_buffers([[maybe_unused]] DrawList& draw_list) -> void {
    draw_list.destroy_buffer(bone_matrices_buffer_name);
  }

  template<typename DrawList>
  static auto update_shared_buffers(DrawList& draw_list) -> void {
    draw_list.update_buffer(_bone_matrices, bone_matrices_buffer_name);

    _bone_matrices.clear();
  }

  template<typename Callable>
  static auto for_each_submission(scenes::scene& scene, Callable&& callable) -> void {
    auto& assets_module = core::engine::get_module<assets::assets_module>();

    _skinning_jobs.clear();

    // pull id to optionally pack selection; animator is present but we only need the pose already stored in component
    const auto query = scene.query<const scenes::skinned_mesh, const scenes::selection_tag, animations::animator>();

    for (auto&& [node, skinned_mesh, selection_tag, animator] : query.each()) {
      const auto transform_data = models::transform_data{scene.world_transform(node), scene.world_normal(node)};

      const auto& pose = skinned_mesh.pose();

      const auto bone_offset = static_cast<std::uint32_t>(_bone_matrices.size());
      const auto bone_count = static_cast<std::uint32_t>(pose.size());

      utility::append(_bone_matrices, pose);

      const auto& mesh_id = skinned_mesh.mesh_id();

      auto& mesh = assets_module.get_asset<animations::mesh>(mesh_id);

      _skinning_jobs.emplace_back(skinning_job{
        .pre_vertices = mesh.address(),
        .post_vertices = 0,
        .vertex_count = mesh.vertex_count(),
        .bone_offset = bone_offset
      });

      for (const auto& submesh : skinned_mesh.submeshes()) {
        std::invoke(callable, node, mesh_id, submesh.index, submesh.material, transform_data, selection_tag, instance_payload{});
      }
    }
  }

  static auto make_instance_data(const scenes::node node, std::uint32_t transform_index, std::uint32_t material_index, const scenes::selection_tag& selection_tag, const instance_payload& payload) -> models::instance_data {
    const auto object_id = (selection_tag != scenes::selection_tag::null()) ? static_cast<std::uint32_t>(node) : std::uint32_t{0u};

    return models::instance_data{transform_index, material_index, object_id, 0u};
  }

  template<typename Mesh, typename Emitter>
  static auto build_draw_commands(const Mesh& mesh, std::uint32_t submesh_index, std::vector<models::instance_data>&& instances, Emitter&& emitter) -> std::uint32_t {
    auto base_instance_offset = std::uint32_t{0};

    const auto& submesh = mesh.submesh(submesh_index);

    for (const auto& instance : instances) {
      const auto instance_index = emitter.base_instance + base_instance_offset;

      auto command = VkDrawIndexedIndirectCommand{};
      command.indexCount = submesh.index_count;
      command.instanceCount = 1u;
      command.firstIndex = submesh.index_offset;
      command.vertexOffset = instance_index * mesh.vertex_count();
      command.firstInstance = instance_index;

      emitter.emit_single(command, instance);

      ++base_instance_offset;
    }

    return base_instance_offset;
  }

  static auto skinning_jobs() -> std::vector<skinning_job>& {
    return _skinning_jobs;
  }

private:

  inline static auto _bone_matrices = std::vector<math::matrix4x4>{};
  inline static auto _skinning_jobs = std::vector<skinning_job>{};

}; // struct skinned_mesh_traits

using skinned_mesh_material_draw_list = models::basic_material_draw_list<skinned_mesh_traits>;

class skinned_mesh_subrenderer final : public graphics::subrenderer {

  inline static const auto pipeline_definition = graphics::pipeline_definition{
    .depth = graphics::depth::read_write,
    .uses_transparency = false,
    .rasterization_state = graphics::rasterization_state{
      .polygon_mode = graphics::polygon_mode::fill,
      .cull_mode = graphics::cull_mode::back,
      .front_face = graphics::front_face::counter_clockwise
    }
  };

public:

  skinned_mesh_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline, const skinned_mesh_material_draw_list::bucket bucket) 
  : graphics::subrenderer{},
    _attachments{attachments},
    _base_pipeline{base_pipeline}, 
    _bucket{bucket},
    _skinning_pipeline{"res://shaders/skinning"},
    _skinning_pipeline_push_handler{_skinning_pipeline} {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    
    _skinning_vertex_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    _skinning_jobs_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  }

  ~skinned_mesh_subrenderer() override {
    _pipeline_cache.clear();
  }

  auto render(graphics::command_buffer& command_buffer) -> void override {
    EASY_FUNCTION();

    SBX_PROFILE_SCOPE("skinned_mesh_subrenderer::render");

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& renderer = graphics_module.renderer();

    auto& assets_module = core::engine::get_module<assets::assets_module>();
    auto& scene = core::engine::get_module<scenes::scenes_module>().scene();

    // obtain the skinned draw list from the pass (name must match your render graph)
    auto& draw_list = renderer.draw_list<skinned_mesh_material_draw_list>("skinned_mesh_material");

    const auto bone_matrices_buffer_address = draw_list.buffer(skinned_mesh_traits::bone_matrices_buffer_name).address();

    _dispatch_skinning(command_buffer, bone_matrices_buffer_address);

    for (auto& [key, data] : draw_list.ranges(_bucket)) {
      auto& pipeline_data = _get_or_create_pipeline(key);
      auto& pipeline = graphics_module.get_resource<graphics::graphics_pipeline>(pipeline_data.pipeline);

      pipeline.bind(command_buffer);

      pipeline_data.scene_descriptor_handler.push("scene", scene.uniform_handler());
      pipeline_data.scene_descriptor_handler.push("samplers", draw_list.samplers());
      pipeline_data.scene_descriptor_handler.push("images", draw_list.images());

      if (!pipeline_data.scene_descriptor_handler.update(pipeline)) {
        return;
      }

      pipeline_data.scene_descriptor_handler.bind_descriptors(command_buffer);

      pipeline_data.push_handler.push("transform_data_buffer", draw_list.buffer(skinned_mesh_material_draw_list::transform_data_buffer_name).address());
      pipeline_data.push_handler.push("material_data_buffer", draw_list.buffer(skinned_mesh_material_draw_list::material_data_buffer_name).address());

      // pipeline_data.push_handler.push("bone_matrices_buffer", bone_matrices_buffer_address);

      pipeline_data.push_handler.push("instance_data_buffer", graphics_module.get_resource<graphics::storage_buffer>(data.instance_data_buffer).address());

      pipeline_data.push_handler.push("vertex_buffer", graphics_module.get_resource<graphics::storage_buffer>(_skinning_vertex_buffer).address());

      for (const auto& draw_range : data.ranges) {
        auto& mesh = assets_module.get_asset<animations::mesh>(draw_range.mesh_id);

        mesh.bind(command_buffer);

        pipeline_data.push_handler.bind(command_buffer);

        auto& draw_commands_buffer = graphics_module.get_resource<graphics::storage_buffer>(data.draw_commands_buffer);

        command_buffer.draw_indexed_indirect(draw_commands_buffer, draw_range.range.offset, draw_range.range.count);
      }
    }
  }

private:

  struct pipeline_data {

    graphics::graphics_pipeline_handle pipeline;
    graphics::push_handler push_handler;
    graphics::descriptor_handler scene_descriptor_handler;

    graphics::compute_pipeline_handle skinning_pipeline;

    pipeline_data(const graphics::graphics_pipeline_handle& handle)
    : pipeline{handle},
      push_handler{pipeline}, 
      scene_descriptor_handler{pipeline, 0u} { }

  }; // struct pipeline_data

  auto _get_or_create_pipeline(const models::material_key& key) -> pipeline_data& {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    if (auto it = _pipeline_cache.find(key); it != _pipeline_cache.end()) {
      return it->second;
    }

    auto definition = pipeline_definition;
    definition.depth = graphics::depth::read_write;
    definition.rasterization_state.cull_mode = key.is_double_sided ? graphics::cull_mode::none : graphics::cull_mode::back;
    definition.uses_transparency = (static_cast<models::alpha_mode>(key.alpha) == models::alpha_mode::blend);

    auto& compiler = graphics_module.compiler();

    const auto request = graphics::compiler::compile_request{
      .path = _base_pipeline,
      .per_stage = {
        {SLANG_STAGE_VERTEX, {.entry_point = "main"}},
        {SLANG_STAGE_FRAGMENT, {.entry_point = _fs_entry.at(key.alpha)}}
      }
    };

    const auto result = compiler.compile(request);

    auto compiled = graphics::graphics_pipeline::compiled_shaders{ _base_pipeline.filename().string(), result.code };
    auto handle = graphics_module.add_resource<graphics::graphics_pipeline>(compiled, _attachments, definition);

    auto [entry, inserted] = _pipeline_cache.emplace(key, handle);

    return entry->second;
  }

  template<typename Type>
  static auto _resize_buffer(graphics::storage_buffer& buffer, std::uint32_t element_count) -> void {
    const std::size_t required_size = static_cast<std::size_t>(element_count) * sizeof(Type);

    if (buffer.size() < required_size) {
      buffer.resize(required_size + required_size / 2);
    }
  }

  template<typename Type>
  static auto _update_buffer(graphics::storage_buffer& buffer, const std::vector<Type>& data) -> void {
    _resize_buffer<Type>(buffer, static_cast<std::uint32_t>(data.size()));

    if (!data.empty()) {
      buffer.update(data.data(), data.size() * sizeof(Type));
    }
  }

  auto _dispatch_skinning(graphics::command_buffer& command_buffer, graphics::buffer::address_type bone_matrices_buffer_address) -> void {
    constexpr auto threads_per_group = std::uint32_t{64};

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    auto& jobs = skinned_mesh_traits::skinning_jobs();
    const auto job_count = static_cast<std::uint32_t>(jobs.size());

    if (jobs.empty()) {
      return;
    }

    auto total_vertices = std::uint32_t{0};
    auto max_vertex_count = std::uint32_t{0};

    for (const auto& job : jobs) {
      total_vertices += job.vertex_count;
      max_vertex_count = std::max(max_vertex_count, job.vertex_count);
    }

    auto& skinning_vertex_buffer = graphics_module.get_resource<graphics::storage_buffer>(_skinning_vertex_buffer);

    _resize_buffer<models::vertex3d>(skinning_vertex_buffer, total_vertices);

    auto address = skinning_vertex_buffer.address();

    for (auto& job : jobs) {
      job.post_vertices = address;
      address += job.vertex_count * sizeof(models::vertex3d);
    }

    auto& skinning_jobs_buffer = graphics_module.get_resource<graphics::storage_buffer>(_skinning_jobs_buffer);

    _update_buffer(skinning_jobs_buffer, jobs);

    _skinning_pipeline.bind(command_buffer);

    _skinning_pipeline_push_handler.push("skinning_jobs", skinning_jobs_buffer.address());
    _skinning_pipeline_push_handler.push("bone_matrices_buffer", bone_matrices_buffer_address);

    _skinning_pipeline_push_handler.bind(command_buffer);

    const auto groups_per_job = (max_vertex_count + threads_per_group - 1) / threads_per_group;

    _skinning_pipeline.dispatch(command_buffer, {job_count, groups_per_job, 1});

    auto barrier_data = graphics::command_buffer::buffer_barrier_data{
      .buffers = {
        skinning_vertex_buffer
      },
      .src_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      .dst_stage_mask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
      .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT,
      .dst_access_mask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
    };

    command_buffer.buffer_barrier(barrier_data);
  }

  // per-alpha fragment entry points (same scheme as your static renderer)
  inline static const auto _fs_entry = std::array<std::string, 3u>{
    "opaque_main",  // alpha_mode::opaque
    "mask_main",    // alpha_mode::mask
    "blend_main"    // alpha_mode::blend
  };

  std::vector<graphics::attachment_description> _attachments;
  std::filesystem::path _base_pipeline;
  skinned_mesh_material_draw_list::bucket _bucket;
  
  graphics::storage_buffer_handle _skinning_vertex_buffer;
  graphics::storage_buffer_handle _skinning_jobs_buffer;

  sbx::graphics::compute_pipeline _skinning_pipeline;
  sbx::graphics::push_handler _skinning_pipeline_push_handler;

  inline static std::unordered_map<models::material_key, pipeline_data, models::material_key_hash> _pipeline_cache{};
  
}; // class skinned_mesh_subrenderer

} // namespace sbx::animation

#endif // LIBSBX_ANIMATIONS_SKINNED_MESH_SUBRENDERER_HPP_
