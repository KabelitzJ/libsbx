// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_MATERIAL_DRAW_LIST_HPP_
#define LIBSBX_MODELS_MATERIAL_DRAW_LIST_HPP_

#include <memory_resource>
#include <execution>
#include <string>

#include <magic_enum/magic_enum.hpp>

#include <libsbx/reflection/description.hpp>

#include <libsbx/math/half.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/memory/tracking_allocator.hpp>

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

enum class bucket : std::uint8_t {
  opaque,
  transparent,
  shadow
}; // enum class bucket

template<typename Traits>
class basic_material_draw_list final : public graphics::draw_list {

  using traits_type = Traits;

public:

  using mesh_type = typename traits_type::mesh_type;
  using instance_payload = typename traits_type::instance_payload;

  using bucket = models::bucket;

  struct range_reference {
    math::uuid mesh_id;
    graphics::draw_command_range range;
  }; // struct range_reference

  struct bucket_entry {
    graphics::storage_buffer_handle draw_commands_buffer{};
    graphics::storage_buffer_handle instance_data_buffer{};
    std::vector<std::uint32_t> command_instance_counts;
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
    SBX_PROFILE_SCOPE("basic_material_draw_list::update");

    SBX_PROFILE_SCOPE_START(s0, "clear data");

    _transform_data.clear();
    _material_data.clear();

    for (auto& [key, pipeline_data] : _pipeline_data) {
      pipeline_data.submesh_instances.clear();
    }

    for (auto& buckets : _bucket_ranges) {
      buckets.clear();
    }

    SBX_PROFILE_SCOPE_END(s0);

    auto& assets_module = core::engine::get_module<assets::assets_module>();

    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.active_scene();
    auto& environment = scene.environment();
    auto& graph = scene.graph();

    SBX_PROFILE_SCOPE_START(s1, "classify submissions");

    auto memory = std::array<std::uint8_t, 4096u>{};
    auto pool = std::pmr::monotonic_buffer_resource{memory.data(), memory.size()};

    auto material_indices = std::pmr::unordered_map<math::uuid, std::uint32_t>{&pool};

    using bucket_set = std::pmr::unordered_set<bucket>;
    auto material_buckets = std::pmr::unordered_map<material_key, bucket_set, material_key_hash>{&pool};

    traits_type::for_each_submission(scene, [&](const scenes::node node, const math::uuid& mesh_id, std::uint32_t submesh_index, const math::uuid& material_id, const transform_data& transform, const instance_payload& payload) {
      const auto transform_index = static_cast<std::uint32_t>(_transform_data.size());
      _transform_data.push_back(transform);

      const auto& material = assets_module.get_asset<models::material>(material_id);

      auto& pipeline = _get_or_create_pipeline_data(material);

      auto [entry, created] = material_indices.try_emplace(material_id, static_cast<std::uint32_t>(_material_data.size()));

      if (created) {
        _push_material(material, material_buckets, pool);
      }

      const auto instance = traits_type::make_instance_data(node, transform_index, entry->second, payload);

      auto& per_mesh = pipeline.submesh_instances[mesh_id];

      per_mesh.resize(std::max(per_mesh.size(), static_cast<std::size_t>(submesh_index + 1u)));
      per_mesh[submesh_index].push_back(instance);
    });

    SBX_PROFILE_SCOPE_END(s1);

    update_buffer(_transform_data, transform_data_buffer_name);
    update_buffer(_material_data, material_data_buffer_name);

    auto camera_node = environment.camera();
    const auto camera_position = graph.world_position(camera_node);

    traits_type::update_shared_buffers(*this);

    SBX_PROFILE_SCOPE_START(s2, "build draw commands");

    for (auto& [key, pipeline_data] : _pipeline_data) {
      if (pipeline_data.submesh_instances.empty()) {
        continue;
      }

      for (auto& [mesh_id, submesh_vectors] : pipeline_data.submesh_instances) {
        for (auto& instances : submesh_vectors) {
          auto sort_function = [&](const instance_data& a, const instance_data& b) {
            const auto& transform_a = _transform_data[a.transform_index];
            const auto& transform_b = _transform_data[b.transform_index];

            const auto position_a = math::vector3(transform_a.model[3]); 
            const auto position_b = math::vector3(transform_b.model[3]);

            const auto distance_sq_a = math::vector3::distance_squared(camera_position, position_a);
            const auto distance_sq_b = math::vector3::distance_squared(camera_position, position_b);

            return distance_sq_a < distance_sq_b;
          };

          std::sort(std::execution::par_unseq, instances.begin(), instances.end(), sort_function);
        }
      }

      _build_draw_commands(key, pipeline_data, material_buckets);
    }

    SBX_PROFILE_SCOPE_END(s2);
  }

  auto ranges(const bucket bucket) const -> const bucket_map& {
    return _bucket_ranges[utility::to_underlying(bucket)];
  }

  static auto surface_shader_path(const std::uint32_t hash) -> std::string {
    if (auto entry = _surface_shader_paths.find(hash); entry != _surface_shader_paths.end()) {
      return entry->second;
    }

    throw utility::runtime_error{"Could not find shader path for hash {}", hash};
  }

private:

  template<typename EmitInstanced, typename EmitSingle>
  struct draw_command_emitter {
    std::uint32_t base_instance;
    EmitInstanced emit_instanced;
    EmitSingle emit_single;
  }; // struct draw_command_emitter

  struct pipeline_data {

    std::unordered_map<math::uuid, std::vector<std::vector<instance_data>>> submesh_instances;

    graphics::storage_buffer_handle draw_commands_buffer;
    graphics::storage_buffer_handle instance_data_buffer;

    pipeline_data() {
      auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
      
      draw_commands_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
      instance_data_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    }

  }; // struct pipeline_data

  static auto _classify_bucket(const models::material& material) -> bucket {
    return (material.alpha == models::alpha_mode::blend) ? bucket::transparent : bucket::opaque;
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

  static auto _pack_half2(std::float_t a, std::float_t b) -> std::float_t {
    auto ha = math::float_to_half(a);
    auto hb = math::float_to_half(b);

    return std::bit_cast<std::float_t>(static_cast<std::uint32_t>(ha) | (static_cast<std::uint32_t>(hb) << 16));
  }

  auto _push_material(const models::material& material, std::pmr::unordered_map<material_key, std::pmr::unordered_set<bucket>, material_key_hash>& material_buckets, std::pmr::monotonic_buffer_resource& pool) -> void {
    auto data = models::material_data{};

    // Images
    data.albedo_image_index = add_image(material.albedo.image);
    data.normal_image_index = add_image(material.normal.image);
    data.metallic_roughness_image_index = add_image(material.metallic_roughness.image);
    data.occlusion_image_index = add_image(material.occlusion.image);
    data.emissive_image_index = add_image(material.emissive.image);
    data.height_image_index = add_image(material.height.image);

    // Samplers
    data.albedo_sampler_index = add_sampler_state(_get_or_create_sampler(material.albedo));
    data.normal_sampler_index = add_sampler_state(_get_or_create_sampler(material.normal));
    data.metallic_roughness_sampler_index = add_sampler_state(_get_or_create_sampler(material.metallic_roughness));
    data.occlusion_sampler_index = add_sampler_state(_get_or_create_sampler(material.occlusion));
    data.emissive_sampler_index = add_sampler_state(_get_or_create_sampler(material.emissive));
    data.height_sampler_index = add_sampler_state(_get_or_create_sampler(material.height));

    // Factors
    data.base_color = material.base_color;
    data.metallic_factor = material.metallic_factor;
    data.roughness_factor = material.roughness_factor;
    data.occlusion_strength = material.occlusion_strength;
    data.normal_scale = material.normal_scale;
    data.emissive_strength = material.emissive_strength;
    data.emissive_factor = material.emissive_factor;
    data.specular_factor = material.specular_factor;
    data.alpha_cutoff = material.alpha_cutoff;
    data.flags = material.features.underlying();

    // Parallax
    data.height_scale = material.height_scale;
    data.height_offset = material.height_offset;
    data.parallax_min_layers = material.parallax_min_layers;
    data.parallax_max_layers = material.parallax_max_layers;

    // UV
    data.uv0_offset = material.uv0_offset;
    data.uv0_scale = material.uv0_scale;

    data.uv1_offset = material.uv1_offset;
    data.uv1_scale = material.uv1_scale;

    data.sway_speed_strength = _pack_half2(material.sway_speed, material.sway_strength);
    data.scrumble_speed_strength = _pack_half2(material.scrumble_speed, material.scrumble_strength);
    data.falloff_exponents = _pack_half2(material.sway_falloff_exponent, material.scrumble_falloff_exponent);

    data.uv_mask = material.uv_mask;

    _material_data.push_back(data);

    const auto key = static_cast<material_key>(material);

    _surface_shader_paths.emplace(key.surface_shader_hash, material.surface_shader.generic_string());

    auto [entry, inserted] = material_buckets.try_emplace(key, std::pmr::unordered_set<bucket>{&pool});

    auto& buckets = entry->second;

    buckets.insert(_classify_bucket(material));

    if (material.features.has(models::material_feature::cast_shadow) && material.alpha != models::alpha_mode::blend) {
      buckets.insert(bucket::shadow);
    }
  }

  auto _build_draw_commands(const material_key& key, pipeline_data& pipeline, const std::pmr::unordered_map<material_key, std::pmr::unordered_set<bucket>, material_key_hash>& material_buckets) -> void {
    SBX_PROFILE_SCOPE("build_draw_commands");

    auto& assets_module = core::engine::get_module<assets::assets_module>();

    auto draw_commands = std::vector<VkDrawIndexedIndirectCommand>{};
    auto instance_data = std::vector<models::instance_data>{};
    auto command_instance_counts = std::vector<std::uint32_t>{};
    auto range = graphics::draw_command_range{};

    const auto& buckets = material_buckets.at(key);

    auto emitter = draw_command_emitter{
      .base_instance = std::uint32_t{0u},
      .emit_instanced = [&](const VkDrawIndexedIndirectCommand& command, std::vector<models::instance_data>&& instances) -> void {
        draw_commands.push_back(command);
        command_instance_counts.push_back(command.instanceCount);
        utility::append(instance_data, std::move(instances));
        range.count++;
      },
      .emit_single = [&](const VkDrawIndexedIndirectCommand& command, const models::instance_data& instance) -> void {
        draw_commands.push_back(command);
        command_instance_counts.push_back(command.instanceCount);
        instance_data.push_back(instance);
        range.count++;
      }
    };

    for (auto& [mesh_id, submesh_vectors] : pipeline.submesh_instances) {
      const auto& mesh = assets_module.get_asset<mesh_type>(mesh_id);

      range.offset = static_cast<std::uint32_t>(draw_commands.size());
      range.count = 0u;

      for (auto&& [submesh_index, instances] : ranges::views::enumerate(submesh_vectors)) {
        emitter.base_instance += traits_type::build_draw_commands(mesh, submesh_index, std::move(instances), emitter);
      }

      if (range.count > 0) {
        const auto hash = material_key_hash{}(key);

        push_draw_command_range(hash, mesh_id, range);

        for (const auto& bucket_type : buckets) {
          auto& entry = _bucket_ranges[utility::to_underlying(bucket_type)][key];

          entry.draw_commands_buffer = pipeline.draw_commands_buffer;
          entry.instance_data_buffer = pipeline.instance_data_buffer;
          entry.ranges.push_back(range_reference{ .mesh_id = mesh_id, .range = range });
        }
      }
    }

    utility::assert_that(emitter.base_instance == instance_data.size(), "build_draw_commands is broken");

    if (!draw_commands.empty()) {
      _update_buffer(pipeline.draw_commands_buffer, draw_commands);
      _update_buffer(pipeline.instance_data_buffer, instance_data);

      for (auto& bucket_ranges : _bucket_ranges) {
        if (auto entry = bucket_ranges.find(key); entry != bucket_ranges.end()) {
          entry->second.command_instance_counts = command_instance_counts;
        }
      }
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

  inline static auto _surface_shader_paths = std::unordered_map<std::uint32_t, std::string>{};

  inline static auto _samplers = std::unordered_map<std::size_t, graphics::sampler_state_handle>{};

}; // class material_draw_list

} // namespace sbx::models

template<>
struct sbx::reflection::description<sbx::models::bucket> {

  static constexpr auto name() -> std::string_view {
    return "bucket";
  }

  static constexpr auto enumerators() {
    return std::make_tuple(
      enumerator{"opaque", sbx::models::bucket::opaque},
      enumerator{"transparent", sbx::models::bucket::transparent},
      enumerator{"shadow", sbx::models::bucket::shadow}
    );
  }

}; // struct sbx::reflection::description<sbx::models::bucket>

#endif // LIBSBX_MODELS_MATERIAL_DRAW_LIST_HPP_
