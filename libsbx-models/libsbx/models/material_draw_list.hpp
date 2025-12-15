#ifndef LIBSBX_MODELS_MATERIAL_DRAW_LIST_HPP_
#define LIBSBX_MODELS_MATERIAL_DRAW_LIST_HPP_

#include <magic_enum/magic_enum.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/draw_list.hpp>

#include <libsbx/graphics/buffers/storage_buffer.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>

#include <libsbx/models/material.hpp>

namespace sbx::models {

struct alignas(16) transform_data {
  math::matrix4x4 model;
  math::matrix4x4 normal;
}; // struct transform_data

struct alignas(16) instance_data {
  std::uint32_t transform_index;
  std::uint32_t material_index;
  std::uint32_t object_id;
  std::uint32_t payload;
}; // struct instance_data

template<typename Traits>
class basic_material_draw_list final : public graphics::draw_list {

  using traits_type = Traits;

public:

  using component_type = typename traits_type::component_type;
  using mesh_type = typename traits_type::mesh_type;
  using instance_payload = typename traits_type::instance_payload;

  enum class bucket : std::uint8_t {
    opaque,
    transparent,
    shadow
  }; // enum class bucket

  struct range_reference {
    math::uuid mesh_id;
    graphics::draw_command_range range;
  }; // struct range_reference

  struct bucket_entry {
    graphics::storage_buffer_handle draw_commands_buffer{};
    graphics::storage_buffer_handle instance_data_buffer{};
    std::vector<range_reference> ranges;
  }; // struct bucket_entry

  using bucket_map = std::unordered_map<material_key, bucket_entry, material_key_hash>;

  inline static const auto transform_data_buffer_name = utility::hashed_string{"transform_data"};
  inline static const auto material_data_buffer_name = utility::hashed_string{"material_data"};

  basic_material_draw_list() {
    create_buffer(transform_data_buffer_name, graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    create_buffer(material_data_buffer_name, graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    traits_type::create_shared_buffers(*this);
  }

  ~basic_material_draw_list() override {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    for (const auto& [key, data] : _pipeline_data) {  
      graphics_module.remove_resource<graphics::storage_buffer>(data.draw_commands_buffer);
      graphics_module.remove_resource<graphics::storage_buffer>(data.instance_data_buffer);
    }

    traits_type::destroy_shared_buffers(*this);
  }

  auto update() -> void override {
    _transform_data.clear();
    _material_data.clear();

    for (auto& [key, pipeline_data] : _pipeline_data) {
      pipeline_data.submesh_instances.clear();
      pipeline_data.allocator.reset();
      pipeline_data.skinning_jobs.clear();
    }

    for (auto& buckets : _bucket_ranges) {
      buckets.clear();
    }

    auto& assets_module = core::engine::get_module<assets::assets_module>();

    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();


    auto material_indices = std::unordered_map<math::uuid, std::uint32_t>{};

    traits_type::for_each_submission(scene, [&](const component_type& component, const math::uuid& mesh_id, std::uint32_t submesh_index, const math::uuid& material_id, const transform_data& transform, const scenes::selection_tag& selection_tag, const instance_payload& payload) {
      const auto transform_index = static_cast<std::uint32_t>(_transform_data.size());
      _transform_data.push_back(transform);

      const auto& material = assets_module.get_asset<models::material>(material_id);

      auto& pipeline = _get_or_create_pipeline_data(material);

      auto [entry, created] = material_indices.try_emplace(material_id, static_cast<std::uint32_t>(_material_data.size()));

      if (created) {
        _push_material(material);
      }

      const auto instance = traits_type::make_instance_data(transform_index, entry->second, selection_tag, payload);

      auto& per_mesh = pipeline.submesh_instances[mesh_id];

      per_mesh.resize(std::max(per_mesh.size(), static_cast<std::size_t>(submesh_index + 1u)));
      per_mesh[submesh_index].push_back(instance);
    });

    update_buffer(_transform_data, transform_data_buffer_name);
    update_buffer(_material_data, material_data_buffer_name);

    traits_type::update_shared_buffers(*this);

    for (auto& [key, pipeline_data] : _pipeline_data) {
      if (pipeline_data.submesh_instances.empty()) {
        continue;
      }

      _build_draw_commands(key, pipeline_data);
    }
  }

  auto ranges(const bucket bucket) const -> const bucket_map& {
    return _bucket_ranges[magic_enum::enum_underlying(bucket)];
  }

private:

  struct vertex_allocator {
    std::uint32_t next_vertex = 0;

    void reset() {
      next_vertex = 0;
    }

    auto allocate(std::uint32_t count) -> std::uint32_t {
      auto base = next_vertex;
      next_vertex += count;

      return base;
    }
  }; // struct vertex_allocator

  struct skinning_job {
    math::uuid mesh_id;
    uint32_t submesh_index;

    std::int32_t instance_offset;
    std::int32_t instance_count;

    std::uint32_t base_vertex;
    std::uint32_t vertex_count;
  }; // struct skinning_job

  struct pipeline_data {

    std::unordered_map<math::uuid, std::vector<std::vector<instance_data>>> submesh_instances;

    graphics::storage_buffer_handle draw_commands_buffer;
    graphics::storage_buffer_handle instance_data_buffer;

    vertex_allocator allocator;
    std::vector<skinning_job> skinning_jobs;

    pipeline_data() {
      auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
      
      draw_commands_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
      instance_data_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    }

  }; // struct pipeline_data

  static auto _classify_bucket(const models::material& material) -> bucket {
    if (material.alpha == models::alpha_mode::blend) {
      return bucket::transparent;
    }

    return bucket::opaque;
  }

  static auto _submits_to_shadow(const models::material& material) -> bool {
    return material.features.has(models::material_feature::cast_shadow);
  }

  auto _get_or_create_pipeline_data(const material_key& key) -> pipeline_data& {
    if (auto entry = _pipeline_data.find(key); entry != _pipeline_data.end()) {
      return entry->second;
    }

    auto [entry, inserted] = _pipeline_data.emplace(key, pipeline_data{});

    return entry->second;
  }

  auto _get_or_create_sampler(const texture_slot& texture_slot) -> graphics::sampler_state_handle {
    auto key = texture_slot_hash{}(texture_slot);

    if (auto entry = _samplers.find(key); entry != _samplers.end()) {
      return entry->second;
    }

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    auto [entry, inserted] = _samplers.emplace(key, graphics_module.add_resource<graphics::sampler_state>(texture_slot.mag_filter, texture_slot.min_filter, texture_slot.address_mode_u, texture_slot.address_mode_v, texture_slot.anisotropy));

    utility::logger<"models">::debug("New sampler created");

    return entry->second;
  }

  auto _push_material(const models::material& material) -> void {
    auto data = models::material_data{};
    data.albedo_image_index = add_image(material.albedo.image);
    data.normal_image_index = add_image(material.normal.image);
    data.mrao_image_index = add_image(material.mrao.image);
    data.emissive_image_index = add_image(material.emissive.image);
    data.height_image_index = add_image(material.height.image);

    data.albedo_sampler_index = add_sampler_state(_get_or_create_sampler(material.albedo));
    data.normal_sampler_index = add_sampler_state(_get_or_create_sampler(material.normal));
    data.mrao_sampler_index = add_sampler_state(_get_or_create_sampler(material.mrao));
    data.emissive_sampler_index = add_sampler_state(_get_or_create_sampler(material.emissive));
    data.height_sampler_index = add_sampler_state(_get_or_create_sampler(material.height));

    data.height_scale = material.height_scale;
    data.height_offset = material.height_offset;
    data.parallax_min_layers = material.parallax_min_layers;
    data.parallax_max_layers = material.parallax_max_layers;
    data.normal_scale = material.normal_scale;
    data.emissive_factor = material.emissive_factor;

    data.base_color = material.base_color;
    data.emissive_strength = material.emissive_strength;

    data.metallic = material.metallic;
    data.roughness = material.roughness;
    data.occlusion = material.occlusion;

    data.uv_offset = material.uv_offset;
    data.uv_scale = material.uv_scale;

    data.alpha_cutoff = material.alpha_cutoff;
    data.flags = material.features.underlying();

    _material_data.push_back(data);

    auto& buckets = _material_buckets[material];
    buckets.insert(_classify_bucket(material));

    if (_submits_to_shadow(material)) {
      buckets.insert(bucket::shadow);
    }
  }


  auto _build_draw_commands(const material_key& key, pipeline_data& pipeline) -> void {
    auto& assets_module = core::engine::get_module<assets::assets_module>();

    auto draw_commands = std::vector<VkDrawIndexedIndirectCommand>{};
    auto instance_data = std::vector<models::instance_data>{};
    auto base_instance = std::uint32_t{0u};

    const auto& buckets = _material_buckets.at(key);

    for (auto& [mesh_id, submesh_vectors] : pipeline.submesh_instances) {
      auto& mesh = assets_module.get_asset<mesh_type>(mesh_id);

      auto range = graphics::draw_command_range{};
      range.offset = static_cast<std::uint32_t>(draw_commands.size());
      range.count  = 0u;

      for (auto&& [submesh_index, instances] : ranges::views::enumerate(submesh_vectors)) {
        if (instances.empty()) {
          continue;
        }

        const auto& submesh = mesh.submesh(submesh_index);

        // const auto vertices_per_instance = submesh.vertex_count;
        // const auto total_vertices = vertices_per_instance * static_cast<uint32_t>(instances.size());

        // const auto base_vertex = pipeline.vertex_allocator.allocate(total_vertices);

        auto command = VkDrawIndexedIndirectCommand{};
        command.indexCount = submesh.index_count;
        command.instanceCount = static_cast<std::uint32_t>(instances.size());
        command.firstIndex = submesh.index_offset;
        command.vertexOffset = submesh.vertex_offset;
        command.firstInstance = base_instance;

        draw_commands.push_back(command);

        // pipeline.skinning_jobs.push_back({
        //   .mesh_id         = mesh_id,
        //   .submesh_index   = static_cast<uint32_t>(submesh_index),
        //   .instance_offset = base_instance,
        //   .instance_count  = command.instanceCount,
        //   .base_vertex     = base_vertex,
        //   .vertex_count    = vertices_per_instance
        // });

        utility::append(instance_data, std::move(instances));

        base_instance += command.instanceCount;
        range.count++;
      }

      if (range.count > 0) {
        const auto hash = material_key_hash{}(key);

        push_draw_command_range(hash, mesh_id, range);

        for (const auto& bucket_type : buckets) {
          auto& entry = _bucket_ranges[magic_enum::enum_underlying(bucket_type)][key];

          entry.draw_commands_buffer = pipeline.draw_commands_buffer;
          entry.instance_data_buffer = pipeline.instance_data_buffer;
          entry.ranges.push_back(range_reference{ .mesh_id = mesh_id, .range = range });
        }
      }
    }

    if (!draw_commands.empty()) {
      _update_buffer(pipeline.draw_commands_buffer, draw_commands);
      _update_buffer(pipeline.instance_data_buffer, instance_data);
    }
  }


  template <typename Type>
  static auto _update_buffer(graphics::storage_buffer_handle handle, const std::vector<Type>& data) -> void {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& buffer = graphics_module.get_resource<graphics::storage_buffer>(handle);

    const auto required_size = static_cast<std::uint32_t>(data.size() * sizeof(Type));

    if (buffer.size() < required_size) {
      buffer.resize(static_cast<std::size_t>(static_cast<std::float_t>(required_size) * 1.5f));
    }

    if (required_size > 0) {
      buffer.update(data.data(), required_size);
    }
  }

  std::vector<transform_data> _transform_data;
  std::vector<material_data> _material_data;

  std::unordered_map<material_key, pipeline_data, material_key_hash> _pipeline_data;

  std::array<bucket_map, magic_enum::enum_count<bucket>()> _bucket_ranges;

  inline static auto _material_buckets = std::unordered_map<material_key, std::unordered_set<bucket>, material_key_hash>{};

  inline static auto _samplers = std::unordered_map<std::size_t, graphics::sampler_state_handle>{};

}; // class material_draw_list

} // namespace sbx::models

#endif // LIBSBX_MODELS_MATERIAL_DRAW_LIST_HPP_
