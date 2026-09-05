// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/scene_renderer_module.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/angle.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/graphics/profiler.hpp>

#include <libsbx/assets/particle_effect.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/components.hpp>

#include <libsbx/render/passes/depth_pre_pass.hpp>
#include <libsbx/render/passes/light_culling_pass.hpp>
#include <libsbx/render/passes/opaque_pass.hpp>
#include <libsbx/render/passes/skybox_pass.hpp>
#include <libsbx/render/passes/grid_pass.hpp>
#include <libsbx/render/passes/debug_draw_pass.hpp>
#include <libsbx/render/passes/tonemap_pass.hpp>
#include <libsbx/render/passes/transparent_accumulate_pass.hpp>
#include <libsbx/render/passes/particle_pass.hpp>
#include <libsbx/render/passes/transparent_resolve_pass.hpp>
#include <libsbx/render/shadow/shadow_pass.hpp>
#include <libsbx/render/shadow/cascade.hpp>
#include <libsbx/render/scene_blit_compositor.hpp>

namespace sbx::render {

struct frame_data {
  math::matrix4x4 view;
  math::matrix4x4 projection;
  math::vector4 camera_position;
  graphics::buffer::address_type light_address;
  std::uint32_t light_count;
  std::uint32_t padding;
  graphics::buffer::address_type material_address;
  std::uint32_t irradiance_index;
  std::uint32_t brdf_lut_index;
  std::uint32_t prefiltered_index;
  std::uint32_t prefiltered_mip_count;
  std::float_t environment_intensity;
  std::float_t ambient_intensity;
  std::uint32_t directional_light_count;
  std::float_t cluster_scale;
  graphics::buffer::address_type cluster_range_address;
  graphics::buffer::address_type cluster_light_index_address;
  std::float_t cluster_bias;
  math::vector2 cluster_tile_size;
  math::vector4 cascade_splits;
  std::array<math::matrix4x4, shadow_cascade_count> light_view_projections;
  std::array<std::uint32_t, shadow_cascade_count> shadow_map_indices;
  std::uint32_t shadow_enabled;
}; // struct frame_data

struct cluster_aabb {
  math::vector4 min_view;
  math::vector4 max_view;
}; // struct cluster_aabb

struct cluster_range {
  std::uint32_t offset;
  std::uint32_t count;
}; // struct cluster_range

struct draw_bucket {
  assets::mesh_handle mesh{};
  std::uint32_t submesh_index{0u};
  assets::material_handle material{};
  std::uint32_t pipeline_id{0u};
  std::vector<transform_data> transforms{};
}; // struct draw_bucket

struct transparent_entry {
  assets::mesh_handle mesh{};
  std::uint32_t submesh_index{0u};
  assets::material_handle material{};
  std::uint32_t pipeline_id{0u};
  transform_data transform{};
}; // struct transparent_entry

scene_renderer_module::scene_renderer_module() {
  _ensure_resources();

  _graph.add_pass<depth_pre_pass>();
  _graph.add_pass<light_culling_pass>();
  _graph.add_pass<shadow_pass>();
  _graph.add_pass<opaque_pass>();
  _graph.add_pass<skybox_pass>();
  _graph.add_pass<grid_pass>();
  _graph.add_pass<debug_draw_pass>();
  _graph.add_pass<transparent_accumulate_pass>();
  _graph.add_pass<particle_pass>();
  _graph.add_pass<transparent_resolve_pass>();
  _graph.add_pass<tonemap_pass>();

  auto& presentation_module = core::engine::get_module<render::presentation_module>();

  presentation_module.set_scene_renderer(this);
  presentation_module.set_compositor(std::make_unique<scene_blit_compositor>(*this));
}

scene_renderer_module::~scene_renderer_module() {
  auto& presentation_module = core::engine::get_module<render::presentation_module>();

  presentation_module.set_scene_renderer(nullptr);
}

auto scene_renderer_module::prepare() -> void {
  _work_packet = _build_packet();
}

auto scene_renderer_module::set_viewport_extent(math::vector2u extent) -> void {
  _viewport_extent = extent;
}

auto scene_renderer_module::set_grid_enabled(bool enabled) -> void {
  _grid_enabled = enabled;
}

auto scene_renderer_module::grid_enabled() const -> bool {
  return _grid_enabled;
}

auto scene_renderer_module::_build_packet() -> render_packet {
  SBX_PROFILE_SCOPE("scene_renderer_module::build_packet");

  auto packet = render_packet{};

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  if (_camera_override) {
    packet.camera = *_camera_override;
  } else if (scene.has_active_camera()) {
    auto camera_node = scene.active_camera();

    const auto& camera = camera_node.get_component<scenes::camera>();
    const auto& world = camera_node.world_matrix();

    packet.camera.view = math::matrix4x4::inverted(world);
    packet.camera.position = math::vector3{world[3]};
    packet.camera.fov_degrees = camera.fov_degrees;
    packet.camera.near_plane = camera.near_plane;
    packet.camera.far_plane = camera.far_plane;
    packet.camera.exposure = camera.exposure;
    packet.camera.is_active = true;
  }

  // Environment/skybox stays scene-authored regardless of which camera_data is actually being
  // rendered with — an editor flying around with the override active should still see the level's
  // own sky/ambient, not go dark just because there's no active-camera skybox to derive from.
  if (scene.has_active_camera()) {
    auto camera_node = scene.active_camera();

    if (camera_node.has_component<scenes::skybox>()) {
      const auto& sky = camera_node.get_component<scenes::skybox>();

      packet.environment = sky.environment;
      packet.environment_intensity = sky.intensity;
      packet.ambient_intensity = sky.ambient_intensity;
    }
  }

  auto opaque = std::map<mesh_key, draw_bucket>{};
  auto transparent = std::vector<transparent_entry>{};

  for (const auto [entity, world, renderer] : scene.query<scenes::world_transform, scenes::mesh_renderer>().each()) {
    if (!renderer.mesh.is_valid()) {
      continue;
    }

    const auto& submeshes = renderer.mesh->submeshes();

    for (auto index = std::uint32_t{0u}; index < submeshes.size(); ++index) {
      if (index >= renderer.materials.size()) {
        continue;
      }

      const auto& material = renderer.materials[index];

      if (!material.is_valid()) {
        continue;
      }

      const auto pipeline_id = material->is_double_sided() ? 1u : 0u;

      // Inverse-transpose, computed once here rather than re-derived per-vertex on the GPU, so
      // normals stay correct under non-uniform scale/skew anywhere in the entity's ancestor chain.
      const auto instance = transform_data{world.matrix, math::matrix4x4::transposed(math::matrix4x4::inverted(world.matrix))};

      if (material->alpha() == assets::alpha_mode::blend) {
        transparent.push_back(transparent_entry{renderer.mesh, index, material, pipeline_id, instance});
      } else {
        auto& bucket = opaque[mesh_key{renderer.mesh->id(), index, material->id()}];
        bucket.mesh = renderer.mesh;
        bucket.submesh_index = index;
        bucket.material = material;
        bucket.pipeline_id = pipeline_id;
        bucket.transforms.push_back(instance);
      }
    }
  }

  packet.opaque_commands.reserve(opaque.size());
  packet.shadow_caster_commands.reserve(opaque.size());

  for (auto& [key, bucket] : opaque) {
    auto command = draw_command{};
    command.mesh = bucket.mesh;
    command.submesh_index = bucket.submesh_index;
    command.material = bucket.material;
    command.instance_count = static_cast<std::uint32_t>(bucket.transforms.size());
    command.transform_offset = static_cast<std::uint32_t>(packet.transforms.size());
    command.pipeline_id = bucket.pipeline_id;

    packet.transforms.insert(packet.transforms.end(), bucket.transforms.begin(), bucket.transforms.end());

    if (bucket.material->casts_shadow()) {
      packet.shadow_caster_commands.push_back(command);
    }

    packet.opaque_commands.push_back(std::move(command));
  }

  packet.transparent_commands.reserve(transparent.size());

  for (const auto& entry : transparent) {
    auto command = draw_command{};
    command.mesh = entry.mesh;
    command.submesh_index = entry.submesh_index;
    command.material = entry.material;
    command.instance_count = 1u;
    command.transform_offset = static_cast<std::uint32_t>(packet.transforms.size());
    command.pipeline_id = entry.pipeline_id;

    packet.transforms.push_back(entry.transform);
    packet.transparent_commands.push_back(std::move(command));
  }

  // Collected separately so the shadow-casting light can be swapped to lights[0] — shadow_pass and
  // the lighting shaders assume the caster is always index 0 when has_shadow_caster is set.
  auto directional_lights = std::vector<light_data>{};
  auto shadow_caster_found = false;
  auto shadow_caster_index = std::size_t{0u};

  for (auto&& [entity, transform, light] : scene.query<scenes::world_transform, scenes::directional_light>().each()) {
    const auto& matrix = transform.matrix;

    auto data = light_data{};
    data.type = light_type::directional;
    data.color = math::vector4{light.color.r(), light.color.g(), light.color.b(), light.intensity};
    data.direction = math::vector4{math::vector3::normalized(math::vector3{-matrix[2].x(), -matrix[2].y(), -matrix[2].z()}), 0.0f};

    if (!shadow_caster_found && light.casts_shadows) {
      shadow_caster_found = true;
      shadow_caster_index = directional_lights.size();
      packet.has_shadow_caster = true;
      packet.shadow_distance = light.shadow_distance;
    }

    directional_lights.push_back(data);
  }

  if (shadow_caster_found && shadow_caster_index != 0u) {
    std::swap(directional_lights.front(), directional_lights[shadow_caster_index]);
  }

  packet.lights.insert(packet.lights.end(), directional_lights.begin(), directional_lights.end());

  packet.directional_light_count = static_cast<std::uint32_t>(packet.lights.size());

  for (auto&& [entity, transform, light] : scene.query<scenes::world_transform, scenes::point_light>().each()) {
    const auto& matrix = transform.matrix;
    auto& out = packet.lights.emplace_back();

    out.type = light_type::point;
    out.color = math::vector4{light.color.r(), light.color.g(), light.color.b(), light.intensity};
    out.position = math::vector4{matrix[3].x(), matrix[3].y(), matrix[3].z(), light.range};
  }

  for (auto&& [entity, transform, light] : scene.query<scenes::world_transform, scenes::spot_light>().each()) {
    const auto& matrix = transform.matrix;
    auto& out = packet.lights.emplace_back();

    out.type = light_type::spot;
    out.color = math::vector4{light.color.r(), light.color.g(), light.color.b(), light.intensity};
    out.position = math::vector4{math::vector3{matrix[3]}, light.range};
    out.direction = math::vector4{math::vector3::normalized(math::vector3{-matrix[2].x(), -matrix[2].y(), -matrix[2].z()}), 0.0f};
    out.inner_cos = std::cos(light.inner_angle);
    out.outer_cos = std::cos(light.outer_angle);
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto delta_time = static_cast<std::float_t>(scenes_module.simulation_delta_time().value());
  const auto time = static_cast<std::float_t>(scenes_module.simulation_time().value());

  packet.delta_time = delta_time;
  packet.time = time;

  auto billboard_buckets = std::map<std::pair<std::uint32_t, assets::emitter_blend_mode>, std::vector<particle_billboard_instance>>{};

  // mesh_key doesn't carry a blend mode (ordinary meshes are either fully opaque or fully
  // transparent, never author-chosen additive/alpha_blend) -- particles need one, so pair it here
  // rather than extending the shared key type for a case only particles have.
  struct particle_mesh_key {
    mesh_key key;
    assets::emitter_blend_mode blend_mode;

    auto operator<(const particle_mesh_key& other) const -> bool {
      if (key < other.key) {
        return true;
      }

      if (other.key < key) {
        return false;
      }

      return blend_mode < other.blend_mode;
    }
  };

  struct particle_mesh_bucket {
    assets::mesh_handle mesh;
    std::uint32_t submesh_index{0u};
    assets::material_handle material;
    assets::emitter_blend_mode blend_mode{};
    std::vector<particle_mesh_instance> instances;
  };

  auto mesh_buckets = std::map<particle_mesh_key, particle_mesh_bucket>{};

  for (auto&& [entity, world, instance] : scene.query<scenes::world_transform, scenes::particle_effect>().each()) {
    if (!instance.effect.is_valid()) {
      continue;
    }

    const auto& configs = instance.effect->emitters();
    const auto emitter_count = std::min(configs.size(), instance.emitters.size());

    for (auto index = std::size_t{0u}; index < emitter_count; ++index) {
      const auto& config = configs[index];
      const auto& runtime = instance.emitters[index];

      if (config.render_mode == assets::particle_render_mode::mesh) {
        if (!config.render_mesh.is_valid() || !assets_module.is_resident(config.render_mesh)) {
          continue;
        }

        const auto& submeshes = config.render_mesh->submeshes();

        for (auto submesh_index = std::uint32_t{0u}; submesh_index < submeshes.size(); ++submesh_index) {
          const auto& submesh = submeshes[submesh_index];
          const auto& material = config.render_material.is_valid() ? config.render_material : submesh.material;

          if (!material.is_valid() || !assets_module.is_resident(material)) {
            continue;
          }

          auto& bucket = mesh_buckets[particle_mesh_key{mesh_key{config.render_mesh->id(), submesh_index, material->id()}, config.blend_mode}];
          bucket.mesh = config.render_mesh;
          bucket.submesh_index = submesh_index;
          bucket.material = material;
          bucket.blend_mode = config.blend_mode;

          for (const auto& particle : runtime.particles) {
            // A single rotation float has no natural 3D axis of its own -- yaw around world/local Y
            // is a simple, well-scoped default (matches how most simple mesh-particle setups look),
            // not an attempt at Unity's full 3D particle rotation.
            const auto model =
              math::matrix4x4::translated(math::matrix4x4::identity, particle.position) *
              math::matrix4x4::rotated(math::matrix4x4::identity, math::vector3{0.0f, 1.0f, 0.0f}, math::radian{particle.rotation}) *
              math::matrix4x4::scaled(math::matrix4x4::identity, math::vector3{particle.size, particle.size, particle.size});

            bucket.instances.push_back(particle_mesh_instance{.model = model, .color = particle.color});
          }
        }

        continue;
      }

      // An assigned-but-not-yet-resident texture has nothing valid to sample -- skip until it loads.
      // No texture at all is a real, supported choice: particle_billboard_instance::texture_index
      // keeps its 0xFFFFFFFFu default, and the shader draws a procedural circular falloff instead.
      if (config.texture.is_valid() && !assets_module.is_resident(config.texture)) {
        continue;
      }

      const auto texture_index = config.texture.is_valid() ? config.texture->index() : 0xFFFFFFFFu;

      auto& bucket = billboard_buckets[{texture_index, config.blend_mode}];

      for (const auto& particle : runtime.particles) {
        bucket.push_back(particle_billboard_instance{
          .position = particle.position,
          .size = particle.size,
          .color = particle.color,
          .rotation = particle.rotation,
          .texture_index = texture_index
        });
      }
    }
  }

  for (auto& [key, instances] : billboard_buckets) {
    // A texture+blend key can end up in the map with zero particles (operator[] default-constructs
    // the bucket before anything is known to push into it) -- skip it, so particle_pass never sees a
    // command with instance_count == 0 and mistakes an empty particle_billboard_instances for "no
    // buffer needed yet".
    if (instances.empty()) {
      continue;
    }

    packet.particle_billboard_commands.push_back(particle_billboard_command{
      .blend_mode = key.second,
      .instance_count = static_cast<std::uint32_t>(instances.size()),
      .instance_offset = static_cast<std::uint32_t>(packet.particle_billboard_instances.size())
    });

    packet.particle_billboard_instances.insert(packet.particle_billboard_instances.end(), instances.begin(), instances.end());
  }

  for (auto& [key, bucket] : mesh_buckets) {
    if (bucket.instances.empty()) {
      continue;
    }

    packet.particle_mesh_commands.push_back(particle_mesh_command{
      .blend_mode = bucket.blend_mode,
      .mesh = bucket.mesh,
      .submesh_index = bucket.submesh_index,
      .material = bucket.material,
      .instance_count = static_cast<std::uint32_t>(bucket.instances.size()),
      .instance_offset = static_cast<std::uint32_t>(packet.particle_mesh_instances.size())
    });

    packet.particle_mesh_instances.insert(packet.particle_mesh_instances.end(), bucket.instances.begin(), bucket.instances.end());
  }

  return packet;
}

auto scene_renderer_module::record(graphics::command_buffer& command_buffer, math::vector2u extent) -> void {
  SBX_PROFILE_SCOPE("scene_renderer_module::record");
  SBX_PROFILE_GPU_SCOPE(command_buffer, "scene_renderer_module::record");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& frame_context = graphics_module.frame_context();
  auto& upload_context = graphics_module.upload_context();

  const auto& packet = _work_packet;

  const auto scene_extent = (_viewport_extent.x() > 0u && _viewport_extent.y() > 0u) ? _viewport_extent : extent;

  assets_module.process_uploads(frame_context.frame_index());
  upload_context.flush(command_buffer, frame_context.frame_index());

  auto irradiance_index = 0xFFFFFFFFu;
  auto brdf_lut_index = 0xFFFFFFFFu;
  auto prefiltered_index = 0xFFFFFFFFu;
  auto prefiltered_mip_count = 0u;

  if (packet.camera.is_active && packet.environment.is_valid() && assets_module.is_resident(packet.environment)) {
    irradiance_index = packet.environment->irradiance_index();
    brdf_lut_index = assets_module.brdf_lut_index();
    prefiltered_index = packet.environment->prefiltered_index();
    prefiltered_mip_count = packet.environment->prefiltered_mip_count();
  }

  _resize_targets(scene_extent);

  _has_rendered = packet.camera.is_active;

  if (!packet.camera.is_active) {
    return;
  }

  const auto slot = utility::fast_mod(frame_context.frame_index(), graphics::swapchain::max_frames_in_flight);

  const auto environment_index = (packet.environment.is_valid() && assets_module.is_resident(packet.environment)) ? packet.environment->radiance_index() : 0xFFFFFFFFu;

  auto context = render_context{
    .command_buffer = memory::make_observer(command_buffer),
    .packet = memory::make_observer<const render_packet>(packet),
    .frame_index = frame_context.frame_index(),
    .slot = static_cast<std::uint32_t>(slot),
    .time = packet.time,
    .delta_time = packet.delta_time,
    .extent = scene_extent,
    .swapchain_extent = extent,
    .environment_index = environment_index,
    .environment_intensity = packet.environment_intensity,
    .ambient_intensity = packet.ambient_intensity,
    .irradiance_index = irradiance_index,
    .brdf_lut_index = brdf_lut_index,
    .prefiltered_index = prefiltered_index,
    .prefiltered_mip_count = prefiltered_mip_count,
    .depth = _depth_image,
    .color = _color_image,
    .color_msaa = _color_msaa_image,
    .color_index = _color_index,
    .final_image = _final_image,
    .final_image_index = _final_image_index,
    .accumulator = _accum_image,
    .accumulator_msaa = _accumulator_msaa_image,
    .accumulator_index = _accumulator_index,
    .revealage = _revealage_image,
    .revealage_msaa = _revealage_msaa_image,
    .revealage_index = _revealage_index
  };

  _prepare_frame(context);
  _graph.execute(context);

  // Transition final_image to shader_read_only_optimal here, once — tonemap_pass is always its
  // last writer, and scene_blit_compositor/ui_system::texture_id both sample it afterward.
  auto& registry = graphics_module.resource_registry();
  auto& final_image = registry.get<graphics::image>(context.final_image);

  auto to_read = graphics::command_buffer::image_transition_data{};
  to_read.image = final_image;
  to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_read.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  to_read.dst_access_mask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  to_read.old_layout = graphics::image_layout::color_attachment_optimal;
  to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
  to_read.aspect_mask = final_image.aspect();
  to_read.layer_count = 1u;
  command_buffer.transition_image_layout(to_read);
}

auto scene_renderer_module::_ensure_resources() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();
  auto& registry = graphics_module.resource_registry();

  _sampler_index = bindless_table.sampler_index(graphics::sampler::create_info{
    .max_anisotropy = 16.0f,
    .name = "Material Sampler"
  });

  _clamp_sampler_index = bindless_table.sampler_index(graphics::sampler::create_info{
    .address_mode_u = graphics::address_mode::clamp_to_edge,
    .address_mode_v = graphics::address_mode::clamp_to_edge,
    .address_mode_w = graphics::address_mode::clamp_to_edge,
    .max_lod = 1.0f,
    .name = "Clamp Sampler"
  });

  _color_index = bindless_table.reserve_sampled_image();

  _final_image_index = bindless_table.reserve_sampled_image();

  _accumulator_index = bindless_table.reserve_sampled_image();

  _revealage_index = bindless_table.reserve_sampled_image();

  _frame_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<frame_data> * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Frame Data"
  });

  const auto frame_base = registry.get<graphics::buffer>(_frame_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _frame_addresses[slot] = frame_base + slot * memory::stride_v<frame_data>;
  }

  _light_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<light_data> * light_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Light Data"
  });

  const auto light_base = registry.get<graphics::buffer>(_light_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _light_addresses[slot] = light_base + slot * light_capacity * memory::stride_v<light_data>;
  }

  _transform_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(transform_data) * transform_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Instance Transforms"
  });

  const auto transform_base = registry.get<graphics::buffer>(_transform_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _transform_addresses[slot] = transform_base + slot * transform_capacity * sizeof(transform_data);
  }

  _cluster_aabb_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<cluster_aabb> * light_culling_pass::cluster_count * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Cluster AABBs"
  });

  const auto cluster_aabb_base = registry.get<graphics::buffer>(_cluster_aabb_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _cluster_aabb_addresses[slot] = cluster_aabb_base + slot * light_culling_pass::cluster_count * memory::stride_v<cluster_aabb>;
  }

  _cluster_range_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<cluster_range> * light_culling_pass::cluster_count * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Cluster Ranges"
  });

  const auto cluster_range_base = registry.get<graphics::buffer>(_cluster_range_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _cluster_range_addresses[slot] = cluster_range_base + slot * light_culling_pass::cluster_count * memory::stride_v<cluster_range>;
  }

  _cluster_light_index_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<std::uint32_t> * cluster_light_index_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Cluster Light Indices"
  });

  const auto cluster_light_index_base = registry.get<graphics::buffer>(_cluster_light_index_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _cluster_light_index_addresses[slot] = cluster_light_index_base + slot * cluster_light_index_capacity * memory::stride_v<std::uint32_t>;
  }

  _cluster_counter_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<std::uint32_t> * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Cluster Counter"
  });

  const auto cluster_counter_base = registry.get<graphics::buffer>(_cluster_counter_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _cluster_counter_addresses[slot] = cluster_counter_base + slot * memory::stride_v<std::uint32_t>;
  }

  // Fixed resolution independent of viewport size — created once here, not in _resize_targets (see shadow_pass).
  for (auto cascade = std::size_t{0u}; cascade < shadow_cascade_count; ++cascade) {
    _shadow_map_images[cascade] = registry.emplace<graphics::image>(graphics::image::create_info{
      .extent = math::vector3u{shadow_map_resolution, shadow_map_resolution, 1u},
      .format = graphics::format::d32_sfloat,
      .usage = graphics::image_usage::depth_stencil_attachment | graphics::image_usage::sampled,
      .samples = graphics::samples::count_1,
      .name = "Shadow Cascade " + std::to_string(cascade)
    });

    _shadow_map_indices[cascade] = bindless_table.reserve_sampled_image();
    bindless_table.write_sampled_image(_shadow_map_indices[cascade], registry.get<graphics::image>(_shadow_map_images[cascade]).view());
  }
}

auto scene_renderer_module::_resize_targets(const math::vector2u extent) -> void {
  if (_target_extent == extent) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& surface = graphics_module.surface();
  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& frame_context = graphics_module.frame_context();
  const auto frame_index = frame_context.frame_index();

  if (_target_extent != math::vector2u{0u, 0u}) {
    registry.retire(_depth_image, frame_index);
    registry.retire(_color_image, frame_index);
    registry.retire(_color_msaa_image, frame_index);
    registry.retire(_final_image, frame_index);
    registry.retire(_accum_image, frame_index);
    registry.retire(_accumulator_msaa_image, frame_index);
    registry.retire(_revealage_image, frame_index);
    registry.retire(_revealage_msaa_image, frame_index);
  }

  _depth_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::d32_sfloat,
    .usage = graphics::image_usage::depth_stencil_attachment,
    .samples = render_pass::sample_count,
    .name = "Depth"
  });

  _color_msaa_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::color_attachment,
    .samples = render_pass::sample_count,
    .name = "HDR Color MSAA"
  });

  _color_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .samples = graphics::samples::count_1,
    .name = "HDR Color Resolve"
  });

  bindless_table.write_sampled_image(_color_index, registry.get<graphics::image>(_color_image).view());

  _accumulator_msaa_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::color_attachment,
    .samples = render_pass::sample_count,
    .name = "Transparent Accumulator MSAA"
  });

  _accum_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .samples = graphics::samples::count_1,
    .name = "Transparent Accumulator Resolve"
  });

  bindless_table.write_sampled_image(_accumulator_index, registry.get<graphics::image>(_accum_image).view());

  _revealage_msaa_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16_sfloat,
    .usage = graphics::image_usage::color_attachment,
    .samples = render_pass::sample_count,
    .name = "Transparent Revealage MSAA"
  });

  _revealage_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16_sfloat,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .samples = graphics::samples::count_1,
    .name = "Transparent Revealage Resolve"
  });

  bindless_table.write_sampled_image(_revealage_index, registry.get<graphics::image>(_revealage_image).view());

  const auto final_image_format = static_cast<graphics::format>(surface.format().format);

  _final_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = final_image_format,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .samples = graphics::samples::count_1,
    .name = "Final Viewport Image"
  });

  bindless_table.write_sampled_image(_final_image_index, registry.get<graphics::image>(_final_image).view());

  _target_extent = extent;

  // Extent-dependent images above just got new resource_handles; the graph's compiled barriers
  // reference concrete handles, so it must recompile here (buffers/shadow maps never change handle).
  _graph.compile(_build_graph_resources());
}

auto scene_renderer_module::_build_graph_resources() const -> graph_resources {
  return graph_resources{
    .extent = _target_extent,
    .depth = _depth_image,
    .color = _color_image,
    .color_msaa = _color_msaa_image,
    .final_image = _final_image,
    .accumulator = _accum_image,
    .accumulator_msaa = _accumulator_msaa_image,
    .revealage = _revealage_image,
    .revealage_msaa = _revealage_msaa_image,
    .shadow_maps = _shadow_map_images,
    .frame_buffer = _frame_buffer,
    .cluster_aabb_buffer = _cluster_aabb_buffer,
    .cluster_range_buffer = _cluster_range_buffer,
    .cluster_light_index_buffer = _cluster_light_index_buffer,
    .cluster_counter_buffer = _cluster_counter_buffer
  };
}

auto scene_renderer_module::_prepare_frame(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& registry = graphics_module.resource_registry();

  const auto aspect = static_cast<std::float_t>(context.extent.x()) / static_cast<std::float_t>(context.extent.y());
  const auto projection = math::matrix4x4::perspective(math::degree{context.packet->camera.fov_degrees}, aspect, context.packet->camera.near_plane, context.packet->camera.far_plane);

  const auto light_count = std::min(static_cast<std::uint32_t>(context.packet->lights.size()), light_capacity);

  if (light_count > 0u) {
    auto& light_buffer = registry.get<graphics::buffer>(_light_buffer);

    light_buffer.write(context.packet->lights.data(), light_count * memory::stride_v<light_data>, context.slot * light_capacity * memory::stride_v<light_data>);
  }

  const auto instance_count = std::min(static_cast<std::uint32_t>(context.packet->transforms.size()), transform_capacity);

  if (instance_count > 0u) {
    auto& transform_buffer = registry.get<graphics::buffer>(_transform_buffer);

    transform_buffer.write(context.packet->transforms.data(), instance_count * sizeof(transform_data), context.slot * transform_capacity * sizeof(transform_data));
  }

  const auto directional_light_count = std::min(context.packet->directional_light_count, light_count);

  const auto& camera = context.packet->camera;
  const auto slice_ratio = std::log2(camera.far_plane / camera.near_plane);
  const auto cluster_scale = static_cast<std::float_t>(light_culling_pass::cluster_dimensions.z()) / slice_ratio;
  const auto cluster_bias = -static_cast<std::float_t>(light_culling_pass::cluster_dimensions.z()) * std::log2(camera.near_plane) / slice_ratio;

  const auto cluster_tile_size = math::vector2{
    static_cast<std::float_t>(context.extent.x()) / static_cast<std::float_t>(light_culling_pass::cluster_dimensions.x()),
    static_cast<std::float_t>(context.extent.y()) / static_cast<std::float_t>(light_culling_pass::cluster_dimensions.y())
  };

  // Reset before this frame's light_culling_pass dispatches its atomic-reserving cull_lights.slang.
  const auto counter_reset = std::uint32_t{0u};
  auto& cluster_counter_buffer = registry.get<graphics::buffer>(_cluster_counter_buffer);
  cluster_counter_buffer.write(&counter_reset, sizeof(counter_reset), context.slot * memory::stride_v<std::uint32_t>);

  // shadow_pass and lighting.slang assume lights[0] is the caster when has_shadow_caster is set (see reordering in _build_packet).
  auto shadow_enabled = std::uint32_t{0u};
  auto cascade_splits = math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
  auto light_view_projections = std::array<math::matrix4x4, shadow_cascade_count>{};

  if (context.packet->has_shadow_caster && directional_light_count > 0u) {
    const auto light_direction = math::vector3{context.packet->lights[0].direction};
    const auto cascades = compute_cascades(camera, aspect, light_direction, context.packet->shadow_distance);

    for (auto i = std::size_t{0u}; i < shadow_cascade_count; ++i) {
      light_view_projections[i] = cascades[i].view_projection;
    }

    cascade_splits = math::vector4{cascades[0].split_distance, cascades[1].split_distance, cascades[2].split_distance, cascades[3].split_distance};
    shadow_enabled = 1u;
  }

  context.has_shadow_caster = shadow_enabled != 0u;
  context.shadow_maps = _shadow_map_images;
  context.shadow_map_indices = _shadow_map_indices;

  auto data = frame_data{};
  data.view = context.packet->camera.view;
  data.projection = projection;
  data.camera_position = math::vector4{context.packet->camera.position, 1.0f};
  data.light_address = _light_addresses[context.slot];
  data.light_count = light_count;
  data.material_address = assets_module.material_buffer_address();
  data.irradiance_index = context.irradiance_index;
  data.brdf_lut_index = context.brdf_lut_index;
  data.prefiltered_index = context.prefiltered_index;
  data.prefiltered_mip_count = context.prefiltered_mip_count;
  data.environment_intensity = context.environment_intensity;
  data.ambient_intensity = context.ambient_intensity;
  data.directional_light_count = directional_light_count;
  data.cluster_scale = cluster_scale;
  data.cluster_range_address = _cluster_range_addresses[context.slot];
  data.cluster_light_index_address = _cluster_light_index_addresses[context.slot];
  data.cluster_bias = cluster_bias;
  data.cluster_tile_size = cluster_tile_size;
  data.cascade_splits = cascade_splits;
  data.light_view_projections = light_view_projections;
  data.shadow_map_indices = _shadow_map_indices;
  data.shadow_enabled = shadow_enabled;

  auto& frame_buffer = registry.get<graphics::buffer>(_frame_buffer);
  frame_buffer.write(&data, sizeof(frame_data), context.slot * memory::stride_v<frame_data>);

  context.frame_address = _frame_addresses[context.slot];
  context.transform_address = _transform_addresses[context.slot];
  context.instance_count = instance_count;
  context.sampler_index = _sampler_index;
  context.clamp_sampler_index = _clamp_sampler_index;
  context.show_grid = _grid_enabled;
  context.inverse_view_projection = math::matrix4x4::inverted(projection * context.packet->camera.view);

  context.cluster_aabb_address = _cluster_aabb_addresses[context.slot];
  context.cluster_range_address = data.cluster_range_address;
  context.cluster_light_index_address = data.cluster_light_index_address;
  context.cluster_counter_address = _cluster_counter_addresses[context.slot];
}

} // namespace sbx::render
