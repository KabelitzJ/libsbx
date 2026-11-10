// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/scene_renderer_module.hpp>

#include <libsbx/async/async_module.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <libsbx/utility/logger.hpp>
#include <variant>
#include <tuple>
#include <vector>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/utility/hash.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/algorithm.hpp>
#include <libsbx/math/angle.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/frustum.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/graphics/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>

#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/animation_graph.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/components.hpp>

#include <libsbx/render/passes/ambient_occlusion_pass.hpp>
#include <libsbx/render/passes/bloom_pass.hpp>
#include <libsbx/render/passes/frustum_cull_pass.hpp>
#include <libsbx/render/passes/hiz_pass.hpp>
#include <libsbx/render/passes/depth_pre_pass.hpp>
#include <libsbx/render/passes/light_culling_pass.hpp>
#include <libsbx/render/passes/opaque_pass.hpp>
#include <libsbx/render/passes/skybox_pass.hpp>
#include <libsbx/render/passes/grid_pass.hpp>
#include <libsbx/render/passes/debug_draw_pass.hpp>
#include <libsbx/render/passes/tonemap_pass.hpp>
#include <libsbx/render/passes/fxaa_pass.hpp>
#include <libsbx/render/passes/selection_outline_pass.hpp>
#include <libsbx/render/passes/canvas_pass.hpp>
#include <libsbx/render/passes/canvas_world_pass.hpp>
#include <libsbx/render/passes/transparent_accumulate_pass.hpp>
#include <libsbx/render/passes/particle_pass.hpp>
#include <libsbx/render/passes/transparent_resolve_pass.hpp>
#include <libsbx/render/particles/particle_pool.hpp>
#include <libsbx/render/particles/particle_simulate_pass.hpp>
#include <libsbx/render/skinning/skin_pass.hpp>
#include <libsbx/render/shadow/shadow_pass.hpp>
#include <libsbx/render/shadow/local_shadow_pass.hpp>
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
  math::vector4 cascade_depth_bias_per_texel;
  math::vector4 cascade_texel_world_size;
  std::array<math::vector4, shadow_cascade_count> cascade_spheres;
  std::array<math::matrix4x4, shadow_cascade_count> light_view_projections;
  std::array<std::uint32_t, shadow_cascade_count> shadow_map_indices;
  std::uint32_t shadow_enabled;
  
  std::array<math::vector4, 6u> frustum_planes;

  std::uint32_t scene_depth_index;
  std::float_t near_plane;
  std::float_t far_plane;
  math::vector2 render_target_size;
  std::uint32_t shadow_sampler_index;
  std::uint32_t shadow_cascade_debug;
  std::float_t shadow_depth_bias;
  std::float_t shadow_normal_bias;
  std::float_t shadow_penumbra_scale;
  std::float_t contact_shadow_length;
  std::uint32_t ambient_occlusion_index;  // 0xFFFFFFFF: off
  std::uint32_t local_shadow_view_count;
  graphics::buffer::address_type local_shadow_view_address;
  std::uint32_t local_shadow_atlas_index;
  std::uint32_t local_shadow_padding;
  std::float_t lod_projection_scale;
  std::float_t lod_error_threshold;
  std::float_t shadow_lod_error_threshold;
  std::uint32_t lod_padding;
  graphics::buffer::address_type occlusion_visibility_address;
  graphics::buffer::address_type occlusion_culled_counter_address;
  std::uint32_t hiz_index;
  std::uint32_t hiz_width;
  std::uint32_t hiz_height;
  std::uint32_t hiz_mip_count;
  std::uint32_t occlusion_enabled;
  std::uint32_t occlusion_padding;
  graphics::buffer::address_type decal_address;
  std::uint32_t decal_count;
  std::uint32_t decal_padding;
  graphics::buffer::address_type probe_address;
  std::uint32_t probe_count;
  std::uint32_t probe_padding;
}; // struct frame_data

// A detail level is used while its simplification error stays under this many pixels on screen, times the project's LOD bias.
inline constexpr auto lod_error_pixels = 1.0f;

// Shadow views accept this much more error: shadows are blurred and seen at an angle, so coarser casters rarely show.
inline constexpr auto shadow_lod_error_scale = 2.0f;

// Replaces @p buffer (retiring the old one) with one holding @p slot_capacity elements per frame slot, and points each slot's address at its region.
auto create_slotted_buffer(graphics::buffer_handle& buffer, std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight>& addresses, std::size_t element_size, std::size_t slot_capacity, graphics::buffer_usage usage, graphics::memory_usage memory, std::string_view name) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  if (buffer.is_valid()) {
    registry.retire(buffer, graphics_module.frame_context().frame_index());
  }

  buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = element_size * slot_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = usage,
    .memory = memory,
    .name = std::string{name}
  });

  const auto base = registry.get<graphics::buffer>(buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    addresses[slot] = base + slot * slot_capacity * element_size;
  }
}

inline constexpr auto skinned_bounds_padding_factor = 0.2f;

struct cluster_aabb {
  math::vector4 min_view;
  math::vector4 max_view;
}; // struct cluster_aabb

// Mirrors frame_data.slang's cluster_range.
struct cluster_range {
  std::uint32_t offset;
  std::uint32_t light_count;
  std::uint32_t decal_count;
  std::uint32_t probe_count;
}; // struct cluster_range

struct billboard_key_hash {
  auto operator()(const std::pair<std::uint32_t, assets::emitter_blend_mode>& key) const noexcept -> std::size_t {
    auto seed = std::hash<std::uint32_t>{}(key.first);
    utility::hash_combine(seed, key.second);
    return seed;
  }
}; // struct billboard_key_hash

struct particle_mesh_key {
  mesh_key key;
  assets::emitter_blend_mode blend_mode;

  auto operator==(const particle_mesh_key& other) const -> bool {
    return key == other.key && blend_mode == other.blend_mode;
  }

  auto operator<(const particle_mesh_key& other) const -> bool {
    if (key < other.key) {
      return true;
    }

    if (other.key < key) {
      return false;
    }

    return blend_mode < other.blend_mode;
  }
}; // struct particle_mesh_key

struct particle_mesh_key_hash {
  auto operator()(const particle_mesh_key& value) const noexcept -> std::size_t {
    auto seed = mesh_key_hash{}(value.key);
    utility::hash_combine(seed, value.blend_mode);
    return seed;
  }
}; // struct particle_mesh_key_hash

struct particle_mesh_bucket {
  assets::mesh_handle mesh;
  std::uint32_t submesh_index{0u};
  assets::material_handle material;
  assets::emitter_blend_mode blend_mode{};
  std::vector<particle_mesh_instance> instances;
}; // struct particle_mesh_bucket

// A fresh packet that takes over the previous one's cleared vector storage, so a steady scene stops reallocating.
auto recycle_packet_storage(render_packet& previous) -> render_packet {
  auto packet = render_packet{};

  const auto recycle = [](auto& target, auto& source) {
    target = std::move(source);
    target.clear();
  };

  recycle(packet.opaque_commands, previous.opaque_commands);
  recycle(packet.transparent_commands, previous.transparent_commands);
  recycle(packet.shadow_caster_commands, previous.shadow_caster_commands);
  recycle(packet.selected_commands, previous.selected_commands);
  recycle(packet.transforms, previous.transforms);
  recycle(packet.lights, previous.lights);
  recycle(packet.local_shadow_views, previous.local_shadow_views);
  recycle(packet.local_shadow_dirty, previous.local_shadow_dirty);
  recycle(packet.particle_billboard_instances, previous.particle_billboard_instances);
  recycle(packet.particle_billboard_commands, previous.particle_billboard_commands);
  recycle(packet.particle_mesh_instances, previous.particle_mesh_instances);
  recycle(packet.particle_mesh_commands, previous.particle_mesh_commands);
  recycle(packet.trail_vertices, previous.trail_vertices);
  recycle(packet.trail_commands, previous.trail_commands);
  recycle(packet.particle_emitters, previous.particle_emitters);
  recycle(packet.joint_matrices, previous.joint_matrices);
  recycle(packet.skin_dispatches, previous.skin_dispatches);

  return packet;
}

auto compute_pipeline_id(const assets::material& material) -> std::uint32_t {
  return (material.is_double_sided() ? 1u : 0u) | (material.shading() == assets::shading_model::unlit ? 2u : 0u);
}

// The command's detail levels: its submesh, then up to max_lod_levels - 1 of the cooked simplifications, coarsest last.
auto set_lod_levels(draw_command& command) -> void {
  command.lod_count = 1u;
  command.lod_errors = {};

  if (!command.mesh.is_valid() || command.submesh_index >= command.mesh->submeshes().size()) {
    return;
  }

  const auto& lods = command.mesh->submeshes()[command.submesh_index].lods;
  const auto count = std::min(lods.size(), static_cast<std::size_t>(max_lod_levels - 1u));

  for (auto level = std::size_t{0u}; level < count; ++level) {
    command.lod_errors[level] = lods[level].error;
  }

  command.lod_count = 1u + static_cast<std::uint32_t>(count);
}

// Lays out one command list's culled output within a cull view: each command's indirect args (one per level), and for non-instanced commands its visible transforms (instance_count per level).
// Returns how many args and transforms the list needs per view.
auto assign_lod_layout(std::vector<draw_command>& commands) -> std::pair<std::uint32_t, std::uint32_t> {
  auto args = std::uint32_t{0u};
  auto transforms = std::uint32_t{0u};

  for (auto& command : commands) {
    set_lod_levels(command);

    command.lod_args_offset = args;
    args += command.lod_count;

    if (!command.instances) {
      command.lod_culled_offset = transforms;
      transforms += command.instance_count * command.lod_count;
    }
  }

  return {args, transforms};
}

// Index range of one detail level of a command's submesh.
auto lod_index_range(const draw_command& command, std::uint32_t level) -> std::pair<std::uint32_t, std::uint32_t> {
  if (!command.mesh.is_valid() || command.submesh_index >= command.mesh->submeshes().size()) {
    return {0u, 0u};
  }

  const auto& submesh = command.mesh->submeshes()[command.submesh_index];

  if (level == 0u || level - 1u >= submesh.lods.size()) {
    return {submesh.index_count, submesh.index_offset};
  }

  const auto& lod = submesh.lods[level - 1u];

  return {lod.index_count, lod.index_offset};
}

template<typename Key>
auto bracket_keys(const std::vector<Key>& keys, std::float_t time) -> std::pair<std::size_t, std::size_t> {
  if (keys.size() == 1u || time <= keys.front().time) {
    return {0u, 0u};
  }

  if (time >= keys.back().time) {
    const auto last = keys.size() - 1u;
    return {last, last};
  }

  for (auto index = std::size_t{0u}; index + 1u < keys.size(); ++index) {
    if (time >= keys[index].time && time <= keys[index + 1u].time) {
      return {index, index + 1u};
    }
  }

  return {0u, 0u};
}

auto sample_vector3_track(const std::vector<assets::animation_key<math::vector3>>& keys, std::float_t time) -> math::vector3 {
  const auto [a, b] = bracket_keys(keys, time);

  if (a == b) {
    return keys[a].value;
  }

  const auto span = keys[b].time - keys[a].time;
  const auto t = (span > 0.0f) ? (time - keys[a].time) / span : 0.0f;

  return math::vector3{
    math::mix(keys[a].value.x(), keys[b].value.x(), t),
    math::mix(keys[a].value.y(), keys[b].value.y(), t),
    math::mix(keys[a].value.z(), keys[b].value.z(), t)
  };
}

auto sample_rotation_track(const std::vector<assets::animation_key<math::quaternion>>& keys, std::float_t time) -> math::quaternion {
  const auto [a, b] = bracket_keys(keys, time);

  if (a == b) {
    return keys[a].value;
  }

  const auto span = keys[b].time - keys[a].time;
  const auto t = (span > 0.0f) ? (time - keys[a].time) / span : 0.0f;

  return math::quaternion::slerp(keys[a].value, keys[b].value, t);
}

auto compose_trs(const math::vector3& translation, const math::quaternion& rotation, const math::vector3& scale) -> math::matrix4x4 {
  const auto translation_matrix = math::matrix4x4::translated(math::matrix4x4::identity, translation);
  const auto scale_matrix = math::matrix4x4::scaled(math::matrix4x4::identity, scale);

  return translation_matrix * math::matrix_cast<math::matrix4x4>(rotation) * scale_matrix;
}

auto mix_vector3(const math::vector3& start, const math::vector3& end, std::float_t t) -> math::vector3 {
  return math::vector3{
    math::mix(start.x(), end.x(), t),
    math::mix(start.y(), end.y(), t),
    math::mix(start.z(), end.z(), t)
  };
}

auto sample_clip_trs(const assets::animation_clip& clip, std::float_t time, const std::vector<assets::skeleton::joint>& joints, std::vector<math::vector3>& translations, std::vector<math::quaternion>& rotations, std::vector<math::vector3>& scales) -> void {
  for (const auto& channel : clip.channels()) {
    if (channel.joint_index >= joints.size()) {
      continue;
    }

    if (!channel.translation_keys.empty()) {
      translations[channel.joint_index] = sample_vector3_track(channel.translation_keys, time);
    }

    if (!channel.rotation_keys.empty()) {
      rotations[channel.joint_index] = sample_rotation_track(channel.rotation_keys, time);
    }

    if (!channel.scale_keys.empty()) {
      scales[channel.joint_index] = sample_vector3_track(channel.scale_keys, time);
    }
  }
}

auto advance_local_time(std::float_t time, std::float_t delta_time, std::float_t speed, std::float_t duration, bool loop) -> std::float_t {
  time += delta_time * speed;

  if (duration <= 0.0f) {
    return 0.0f;
  }

  if (loop) {
    time = std::fmod(time, duration);

    if (time < 0.0f) {
      time += duration;
    }

    return time;
  }

  return std::clamp(time, 0.0f, duration);
}

[[nodiscard]] auto find_animation_state(const assets::animation_graph& graph, std::uint32_t state_id) -> const assets::animation_state* {
  const auto& states = graph.states();
  const auto entry = std::ranges::find(states, state_id, &assets::animation_state::id);
  return (entry != states.end()) ? &*entry : nullptr;
}

[[nodiscard]] auto evaluate_animation_condition(const assets::animation_condition& condition, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters) -> bool {
  const auto entry = std::ranges::find(parameters, condition.parameter_name, [](const auto& pair) { return std::string_view{pair.first}; });

  if (entry == parameters.end()) {
    return false;
  }

  const auto& value = entry->second;

  if (const auto* trigger = std::get_if<assets::animation_trigger>(&value)) {
    return trigger->set;
  }

  return std::visit([&condition](const auto& current) -> bool {
    using current_type = std::decay_t<decltype(current)>;

    if constexpr (std::is_same_v<current_type, assets::animation_trigger>) {
      return false;
    } else {
      const auto* expected = std::get_if<current_type>(&condition.expected);

      if (expected == nullptr) {
        return false;
      }

      switch (condition.comparator) {
        case assets::animation_condition_comparator::equals: return current == *expected;
        case assets::animation_condition_comparator::not_equals: return !(current == *expected);
        case assets::animation_condition_comparator::greater: return current > *expected;
        case assets::animation_condition_comparator::greater_or_equal: return current >= *expected;
        case assets::animation_condition_comparator::less: return current < *expected;
        case assets::animation_condition_comparator::less_or_equal: return current <= *expected;
      }

      return false;
    }
  }, value);
}

auto reconcile_animator_parameters(const assets::animation_graph& graph, scenes::animator& animator) -> void {
  for (const auto& parameter : graph.parameters()) {
    const auto exists = std::ranges::any_of(animator.parameters, [&parameter](const auto& entry) { return entry.first == parameter.name; });

    if (!exists) {
      animator.parameters.emplace_back(parameter.name, parameter.default_value);
    }
  }

  std::erase_if(animator.parameters, [&graph](const auto& entry) {
    return !std::ranges::any_of(graph.parameters(), [&entry](const auto& parameter) { return parameter.name == entry.first; });
  });
}

scene_renderer_module::scene_renderer_module() {
  _ensure_resources();

  _particle_pool_additive = std::make_unique<particle_pool>(particle_pool::create_info{
    .max_particles = 65536u,
    .max_emitter_instances = 64u,
    .name = "Particle Pool Additive"
  });

  _particle_pool_alpha_blend = std::make_unique<particle_pool>(particle_pool::create_info{
    .max_particles = 65536u,
    .max_emitter_instances = 64u,
    .name = "Particle Pool Alpha Blend"
  });

  _sample_count = _requested_sample_count();
  _build_graph();

  // GPU particles live in render-side pools, not in the scene, so a replaced world would keep showing the old ones.
  core::engine::get_module<scenes::scenes_module>().on_scene_loaded().connect([this](const scenes::scene_loaded_event& event) {
    if (event.mode == scenes::load_scene_mode::single) {
      reset_particles();
    }
  });

  auto& presentation_module = core::engine::get_module<render::presentation_module>();

  presentation_module.set_scene_renderer(this);
  presentation_module.set_compositor(std::make_unique<scene_blit_compositor>(*this));
}

scene_renderer_module::~scene_renderer_module() {
  auto& presentation_module = core::engine::get_module<render::presentation_module>();

  presentation_module.set_scene_renderer(nullptr);

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  // The graph's passes and query pools are still referenced by in-flight frames.
  graphics_module.logical_device().wait_idle();

  for (const auto view : _shadow_map_preview_views) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(graphics_module.logical_device(), view, nullptr);
    }
  }

  if (_local_shadow_atlas_preview_view != VK_NULL_HANDLE) {
    vkDestroyImageView(graphics_module.logical_device(), _local_shadow_atlas_preview_view, nullptr);
  }

  for (const auto view : _hiz_mip_views) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(graphics_module.logical_device(), view, nullptr);
    }
  }
}

auto scene_renderer_module::prepare() -> void {
  core::engine::get_module<assets::assets_module>().drain_loader_results();

  _advance_capture();

  _work_packet = _build_packet();

  const auto summarize = [](const std::vector<draw_command>& commands) -> draw_category_stats {
    auto stats = draw_category_stats{};

    stats.draw_calls = static_cast<std::uint32_t>(commands.size());

    for (const auto& command : commands) {
      stats.instance_count += command.instance_count;
    }

    return stats;
  };

  _last_draw_stats.opaque = summarize(_work_packet.opaque_commands);
  _last_draw_stats.transparent = summarize(_work_packet.transparent_commands);
  _last_draw_stats.shadow = summarize(_work_packet.shadow_caster_commands);
}

auto scene_renderer_module::set_viewport_extent(math::vector2u extent) -> void {
  _viewport_extent = extent;
}

auto scene_renderer_module::set_viewport_offset(math::vector2 offset) -> void {
  _viewport_offset = offset;
}

auto scene_renderer_module::set_grid_enabled(bool enabled) -> void {
  _grid_enabled = enabled;
}

auto scene_renderer_module::grid_enabled() const -> bool {
  return _grid_enabled;
}

auto scene_renderer_module::set_wireframe_enabled(bool enabled) -> void {
  _wireframe_enabled = enabled;
}

auto scene_renderer_module::wireframe_enabled() const -> bool {
  return _wireframe_enabled;
}

auto scene_renderer_module::set_selected_nodes(std::span<const math::uuid> ids) -> void {
  _selected_nodes.clear();
  _selected_nodes.insert(ids.begin(), ids.end());
}

auto scene_renderer_module::set_shadow_cascade_debug_enabled(bool enabled) -> void {
  _shadow_cascade_debug_enabled = enabled;
}

auto scene_renderer_module::shadow_cascade_debug_enabled() const -> bool {
  return _shadow_cascade_debug_enabled;
}

auto scene_renderer_module::reset_particles() -> void {
  _particle_pool_additive->clear();
  _particle_pool_alpha_blend->clear();
}

auto scene_renderer_module::_extract_gpu_particle_emitter(render_packet& packet, const assets::particle_emitter& config, scenes::particle_emitter& runtime, const scenes::particle_effect& instance, const math::matrix4x4& world, std::float_t delta_time) -> void {
  if (instance.playback != scenes::particle_playback_state::playing) {
    return;
  }

  const auto pool_index = config.blend_mode == assets::emitter_blend_mode::alpha_blend ? particle_pool_alpha_blend : particle_pool_additive;
  auto& pool = pool_index == particle_pool_alpha_blend ? *_particle_pool_alpha_blend : *_particle_pool_additive;

  if (runtime.slot == scenes::particle_emitter::invalid_slot) {
    const auto claimed = pool.claim_slot();

    if (!claimed) {
      return;
    }

    runtime.slot = *claimed;
  }

  const auto emitting = instance.loop || instance.elapsed < instance.duration;

  auto particles_to_emit = std::uint32_t{0u};

  if (emitting) {
    runtime.emission_accumulator += config.emission_rate * delta_time;

    while (runtime.emission_accumulator >= 1.0f) {
      runtime.emission_accumulator -= 1.0f;
      ++particles_to_emit;
    }

    if (config.burst_count > 0u && !runtime.burst_fired) {
      particles_to_emit += config.burst_count;
      runtime.burst_fired = true;
    }
  }

  pool.keep_alive(runtime.slot, config.lifetime_max);

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto texture_index = (config.texture.is_valid() && assets_module.is_resident(config.texture)) ? config.texture->index() : particle_texture_index_none;

  auto data = emitter_instance{};
  data.position = math::vector3{world[3]};
  data.emission_rate = config.emission_rate;
  data.velocity_min = config.velocity_min;
  data.lifetime_min = config.lifetime_min;
  data.velocity_max = config.velocity_max;
  data.lifetime_max = config.lifetime_max;
  data.start_color = math::vector4{config.start_color.r(), config.start_color.g(), config.start_color.b(), config.start_color.a()};
  data.end_color = math::vector4{config.end_color.r(), config.end_color.g(), config.end_color.b(), config.end_color.a()};
  data.size_min = config.size_min;
  data.size_max = config.size_max;
  data.gravity = config.gravity;
  data.drag = config.drag;
  data.active = 1u;
  data.particles_to_emit = particles_to_emit;
  data.seed = runtime.slot * 2654435761u; // combined with push.time in emit.slang
  data.shape = static_cast<std::uint32_t>(config.shape);
  data.shape_extents = config.shape_extents;
  data.texture_index = texture_index;

  packet.particle_emitters.push_back(particle_emitter_snapshot{pool_index, runtime.slot, data});
}

auto scene_renderer_module::_resolve_state_clip(const scenes::mesh_renderer& renderer, const assets::animation_state* state) const -> assets::animation_clip_handle {
  if (state == nullptr || !renderer.mesh.is_valid()) {
    return assets::animation_clip_handle{};
  }

  for (const auto& clip : renderer.mesh->animation_clips()) {
    if (clip.is_valid() && clip->name() == state->clip_name) {
      return clip;
    }
  }

  return assets::animation_clip_handle{};
}

auto scene_renderer_module::_advance_animator_state(const scenes::mesh_renderer& renderer, const assets::animation_graph& graph, scenes::animator& animator, std::float_t delta_time) -> void {
  reconcile_animator_parameters(graph, animator);

  const auto* current_state = find_animation_state(graph, animator.current_state_id);

  if (!animator.current_clip.is_valid()) {
    animator.current_clip = _resolve_state_clip(renderer, current_state);
  }

  if (!animator.transition_target_state_id.has_value()) {
    if (animator.playing && current_state != nullptr) {
      const auto duration = animator.current_clip.is_valid() ? animator.current_clip->duration() : 0.0f;
      animator.current_time = advance_local_time(animator.current_time, delta_time, current_state->speed, duration, current_state->loop);
    }

    for (const auto& transition : graph.transitions()) {
      if (transition.from_state.has_value() && *transition.from_state != animator.current_state_id) {
        continue;
      }

      if (transition.has_exit_time) {
        const auto duration = animator.current_clip.is_valid() ? animator.current_clip->duration() : 0.0f;
        const auto normalized_time = (duration > 0.0f) ? (animator.current_time / duration) : 0.0f;

        if (normalized_time < transition.exit_time) {
          continue;
        }
      }

      const auto conditions_pass = std::ranges::all_of(transition.conditions, [&animator](const auto& condition) {
        return evaluate_animation_condition(condition, animator.parameters);
      });

      if (!conditions_pass) {
        continue;
      }

      animator.transition_target_state_id = transition.to_state;
      animator.transition_time = 0.0f;
      animator.transition_target_time = 0.0f;
      animator.transition_duration = transition.duration;
      animator.transition_target_clip = _resolve_state_clip(renderer, find_animation_state(graph, transition.to_state));

      break;
    }
  } else {
    const auto* target_state = find_animation_state(graph, *animator.transition_target_state_id);

    if (animator.playing) {
      const auto source_duration = animator.current_clip.is_valid() ? animator.current_clip->duration() : 0.0f;
      const auto target_duration = animator.transition_target_clip.is_valid() ? animator.transition_target_clip->duration() : 0.0f;

      animator.current_time = advance_local_time(animator.current_time, delta_time, current_state != nullptr ? current_state->speed : 1.0f, source_duration, current_state == nullptr || current_state->loop);
      animator.transition_target_time = advance_local_time(animator.transition_target_time, delta_time, target_state != nullptr ? target_state->speed : 1.0f, target_duration, target_state == nullptr || target_state->loop);
      animator.transition_time += delta_time;
    }

    const auto alpha = (animator.transition_duration > 0.0f) ? std::clamp(animator.transition_time / animator.transition_duration, 0.0f, 1.0f) : 1.0f;

    if (alpha >= 1.0f) {
      animator.current_state_id = *animator.transition_target_state_id;
      animator.current_time = animator.transition_target_time;
      animator.current_clip = animator.transition_target_clip;

      animator.transition_target_state_id.reset();
      animator.transition_time = 0.0f;
      animator.transition_target_time = 0.0f;
      animator.transition_duration = 0.0f;
      animator.transition_target_clip = assets::animation_clip_handle{};
    }
  }

  for (auto& [name, value] : animator.parameters) {
    if (auto* trigger = std::get_if<assets::animation_trigger>(&value)) {
      trigger->set = false;
    }
  }
}

auto scene_renderer_module::_evaluate_skeleton_pose(const assets::skeleton& skeleton, const scenes::mesh_renderer& renderer, memory::observer_ptr<scenes::animator> animator, scenes::skeleton_pose& pose, std::float_t delta_time, pose_scratch& scratch) -> void {
  const auto& joints = skeleton.joints();
  const auto joint_count = joints.size();

  pose.joint_world_matrices.resize(joint_count);
  pose.skinning_matrices.resize(joint_count);

  const auto* graph = (animator != nullptr && animator->graph.is_valid()) ? animator->graph.get() : nullptr;

  if (graph != nullptr) {
    _advance_animator_state(renderer, *graph, *animator, delta_time);
  }

  auto& translations = scratch.translations;
  auto& rotations = scratch.rotations;
  auto& scales = scratch.scales;

  translations.resize(joint_count);
  rotations.resize(joint_count);
  scales.resize(joint_count);

  for (auto index = std::size_t{0u}; index < joint_count; ++index) {
    const auto& joint = joints[index];
    translations[index] = joint.bind_local_translation;
    rotations[index] = joint.bind_local_rotation;
    scales[index] = joint.bind_local_scale;
  }

  const auto* clip = (graph != nullptr && animator->current_clip.is_valid()) ? animator->current_clip.get() : nullptr;

  if (clip != nullptr) {
    sample_clip_trs(*clip, animator->current_time, joints, translations, rotations, scales);
  }

  if (graph != nullptr && animator->transition_target_state_id.has_value() && animator->transition_target_clip.is_valid()) {
    const auto alpha = (animator->transition_duration > 0.0f) ? std::clamp(animator->transition_time / animator->transition_duration, 0.0f, 1.0f) : 1.0f;

    auto& target_translations = scratch.target_translations;
    auto& target_rotations = scratch.target_rotations;
    auto& target_scales = scratch.target_scales;

    target_translations.resize(joint_count);
    target_rotations.resize(joint_count);
    target_scales.resize(joint_count);

    for (auto index = std::size_t{0u}; index < joint_count; ++index) {
      const auto& joint = joints[index];
      target_translations[index] = joint.bind_local_translation;
      target_rotations[index] = joint.bind_local_rotation;
      target_scales[index] = joint.bind_local_scale;
    }

    sample_clip_trs(*animator->transition_target_clip, animator->transition_target_time, joints, target_translations, target_rotations, target_scales);

    for (auto index = std::size_t{0u}; index < joint_count; ++index) {
      translations[index] = mix_vector3(translations[index], target_translations[index], alpha);
      rotations[index] = math::quaternion::slerp(rotations[index], target_rotations[index], alpha);
      scales[index] = mix_vector3(scales[index], target_scales[index], alpha);
    }
  }

  auto& locals = scratch.locals;
  locals.resize(joint_count);

  for (auto index = std::size_t{0u}; index < joint_count; ++index) {
    locals[index] = compose_trs(translations[index], rotations[index], scales[index]);
  }

  for (auto index = std::size_t{0u}; index < joint_count; ++index) {
    const auto parent_index = joints[index].parent_index;

    pose.joint_world_matrices[index] = (parent_index < 0) ? locals[index] : pose.joint_world_matrices[static_cast<std::size_t>(parent_index)] * locals[index];

    pose.skinning_matrices[index] = pose.joint_world_matrices[index] * joints[index].inverse_bind_matrix;
  }
}

auto scene_renderer_module::_resolve_camera_data() -> camera_data {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  if (_camera_override) {
    return *_camera_override;
  }

  auto data = camera_data{};

  if (scene.has_active_camera()) {
    auto camera_node = scene.active_camera();

    const auto& camera = camera_node.get_component<scenes::camera>();
    const auto& world = camera_node.world_matrix();

    data.view = math::matrix4x4::inverted(world);
    data.position = math::vector3{world[3]};
    data.fov_degrees = camera.fov_degrees;
    data.near_plane = camera.near_plane;
    data.far_plane = camera.far_plane;
    data.post_process = camera.post_process;
    data.is_active = true;
  }

  return data;
}

auto scene_renderer_module::effective_camera() -> camera_data {
  return _resolve_camera_data();
}

auto scene_renderer_module::_build_packet() -> render_packet {
  SBX_PROFILE_SCOPE("scene_renderer_module::build_packet");

  // prepare() runs while the render thread is idle, so the previous packet's storage is free.
  auto packet = recycle_packet_storage(_work_packet);

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  packet.camera = _resolve_camera_data();

  if (_capture_face) {
    const auto& job = _capture_jobs.front();

    packet.camera.view = cubemap_face_view(job.position, *_capture_face);
    packet.camera.position = job.position;
    packet.camera.fov_degrees = 90.0f;
    packet.camera.near_plane = job.near_plane;
    packet.camera.far_plane = job.far_plane;
    packet.camera.is_active = true;
  }

  _select_local_shadows(scene, packet);

  if (scene.has_active_camera()) {
    auto camera_node = scene.active_camera();

    if (camera_node.has_component<scenes::skybox>()) {
      const auto& sky = camera_node.get_component<scenes::skybox>();

      packet.environment = sky.environment;
      packet.environment_intensity = sky.intensity;
      packet.ambient_intensity = sky.ambient_intensity;
    }

    // The editor camera still uses the scene camera's post processing, so the viewport looks like the game.
    if (_camera_override) {
      packet.camera.post_process = camera_node.get_component<scenes::camera>().post_process;
    }
  }

  std::erase_if(_opaque_buckets, [](const auto& entry) { return entry.second.transforms.empty(); });

  for (auto& [key, bucket] : _opaque_buckets) {
    bucket.transforms.clear();
  }

  _transparent_entries.clear();
  _selected_entries.clear();

  // A node is outlined when it or any ancestor is selected, so selecting a parent outlines its whole subtree.
  const auto is_selected = [&](ecs::entity entity) -> bool {
    if (_selected_nodes.empty()) {
      return false;
    }

    for (auto current = entity; current != ecs::null_entity;) {
      auto node = scene.node_of(current);

      // The scene root has a relationship but no id.
      const auto id = node.try_get_component<scenes::id>();

      if (!id) {
        return false;
      }

      if (_selected_nodes.contains(*id)) {
        return true;
      }

      current = node.get_component<scenes::relationship>().parent;
    }

    return false;
  };

  for (const auto [entity, world, renderer] : scene.query<scenes::world_transform, scenes::mesh_renderer>(ecs::exclude<scenes::skeleton_pose, scenes::inactive>).each()) {
    if (!renderer.mesh.is_valid()) {
      continue;
    }

    if (renderer.mesh.is_loaded() && renderer.mesh->skeleton().is_valid()) {
      scene.node_of(entity).get_or_add_component<scenes::skeleton_pose>().skeleton = renderer.mesh->skeleton();
      continue;
    }

    const auto& submeshes = renderer.mesh->submeshes();

    const auto instance = transform_data{world.matrix, math::matrix4x4::transposed(math::matrix4x4::inverted(world.matrix))};
    const auto selected = is_selected(entity);

    for (auto index = std::uint32_t{0u}; index < submeshes.size(); ++index) {
      if (index >= renderer.materials.size()) {
        continue;
      }

      const auto& material = renderer.materials[index];

      if (!material.is_valid()) {
        continue;
      }

      if (selected) {
        _selected_entries.push_back(transparent_entry{renderer.mesh, index, material, compute_pipeline_id(*material), instance});
      }

      if (material->alpha() == assets::alpha_mode::blend) {
        _transparent_entries.push_back(transparent_entry{renderer.mesh, index, material, compute_pipeline_id(*material), instance});
      } else {
        auto& bucket = _opaque_buckets[mesh_key{renderer.mesh->id(), index, material->id()}];

        // Refreshed once per bucket: its instances share mesh and material, and a reused bucket may hold last frame's handles.
        if (bucket.transforms.empty()) {
          bucket.mesh = renderer.mesh;
          bucket.submesh_index = index;
          bucket.material = material;
          bucket.pipeline_id = compute_pipeline_id(*material);
        }

        bucket.transforms.push_back(instance);
      }
    }
  }

  _ordered_opaque_buckets.clear();

  for (const auto& [key, bucket] : _opaque_buckets) {
    if (!bucket.transforms.empty()) {
      _ordered_opaque_buckets.emplace_back(key, &bucket);
    }
  }

  std::ranges::sort(_ordered_opaque_buckets, [](const mesh_key& lhs, const mesh_key& rhs) { return lhs < rhs; }, &std::pair<mesh_key, const draw_bucket*>::first);

  packet.opaque_commands.reserve(_ordered_opaque_buckets.size());
  packet.shadow_caster_commands.reserve(_ordered_opaque_buckets.size());

  for (const auto& [key, bucket_pointer] : _ordered_opaque_buckets) {
    const auto& bucket = *bucket_pointer;

    auto command = draw_command{};
    command.mesh = bucket.mesh;
    command.submesh_index = bucket.submesh_index;
    command.material = bucket.material;
    command.instance_count = static_cast<std::uint32_t>(bucket.transforms.size());
    command.transform_offset = static_cast<std::uint32_t>(packet.transforms.size());
    command.pipeline_id = bucket.pipeline_id;
    command.resident = assets_module.is_resident(bucket.mesh) && assets_module.is_resident(bucket.material);
    command.local_bounds = bucket.mesh->submeshes()[bucket.submesh_index].bounds;

    packet.transforms.insert(packet.transforms.end(), bucket.transforms.begin(), bucket.transforms.end());

    if (bucket.material->casts_shadow()) {
      packet.shadow_caster_commands.push_back(command);
    }

    packet.opaque_commands.push_back(std::move(command));
  }

  // Instanced renderers: one command per submesh, reserving culled pool space per cull view (so local shadow views must be chosen before this).
  auto instanced_culled = std::uint32_t{0u};

  for (const auto [entity, world, renderer] : scene.query<scenes::world_transform, scenes::instanced_mesh_renderer>(ecs::exclude<scenes::inactive>).each()) {
    if (!renderer.mesh.is_valid() || !renderer.material.is_valid() || !renderer.instances || renderer.instances->count() == 0u) {
      continue;
    }

    if (renderer.material->alpha() == assets::alpha_mode::blend) {
      static auto warned = false;

      if (!std::exchange(warned, true)) {
        utility::logger<"render">::warn("instanced_mesh_renderer: blended materials aren't supported, skipping");
      }

      continue;
    }

    const auto count = renderer.instances->count();
    const auto casts_shadow = renderer.material->casts_shadow();
    const auto views = casts_shadow ? cull_view_count(packet) : 1u;
    const auto& submeshes = renderer.mesh->submeshes();

    const auto node_index = static_cast<std::uint32_t>(packet.transforms.size());
    packet.transforms.push_back(transform_data{world.matrix, math::matrix4x4::transposed(math::matrix4x4::inverted(world.matrix))});

    const auto resident = assets_module.is_resident(renderer.mesh) && assets_module.is_resident(renderer.material);

    for (auto index = std::uint32_t{0u}; index < submeshes.size(); ++index) {
      auto command = draw_command{};
      command.mesh = renderer.mesh;
      command.submesh_index = index;
      command.material = renderer.material;
      command.instance_count = count;
      command.transform_offset = node_index;
      command.pipeline_id = compute_pipeline_id(*renderer.material);
      command.resident = resident;
      command.local_bounds = submeshes[index].bounds;
      command.instances = renderer.instances;
      command.culled_offset = instanced_culled;

      // One block per cull view and detail level, plus the late camera view's.
      set_lod_levels(command);
      command.instanced_view_blocks = views + 1u;
      instanced_culled += count * command.instanced_view_blocks * command.lod_count;

      if (casts_shadow) {
        packet.shadow_caster_commands.push_back(command);
      }

      packet.opaque_commands.push_back(std::move(command));
    }
  }

  packet.instanced_culled_count = instanced_culled;

  packet.transparent_commands.reserve(_transparent_entries.size());

  for (const auto& entry : _transparent_entries) {
    auto command = draw_command{};
    command.mesh = entry.mesh;
    command.submesh_index = entry.submesh_index;
    command.material = entry.material;
    command.instance_count = 1u;
    command.transform_offset = static_cast<std::uint32_t>(packet.transforms.size());
    command.pipeline_id = entry.pipeline_id;
    command.resident = assets_module.is_resident(entry.mesh) && assets_module.is_resident(entry.material);

    packet.transforms.push_back(entry.transform);
    packet.transparent_commands.push_back(std::move(command));
  }

  // Unbatched: opaque buckets merge selected and unselected instances, so selected ones get their own draws.
  packet.selected_commands.reserve(_selected_entries.size());

  for (const auto& entry : _selected_entries) {
    auto command = draw_command{};
    command.mesh = entry.mesh;
    command.submesh_index = entry.submesh_index;
    command.material = entry.material;
    command.instance_count = 1u;
    command.transform_offset = static_cast<std::uint32_t>(packet.transforms.size());
    command.pipeline_id = entry.pipeline_id;
    command.resident = assets_module.is_resident(entry.mesh) && assets_module.is_resident(entry.material);

    packet.transforms.push_back(entry.transform);
    packet.selected_commands.push_back(std::move(command));
  }

  auto skin_scratch_cursor = std::uint32_t{0u};
  const auto animation_delta_time = scenes_module.simulation_delta_time().value();

  // prepare() runs before this packet's frame begins, so this is the frame_index record() will see.
  const auto& frame_context = core::engine::get_module<graphics::graphics_module>().frame_context();
  const auto skin_scratch_address = _skin_scratch_addresses[utility::fast_mod(frame_context.frame_index(), graphics::swapchain::max_frames_in_flight)];

  _skinned_entries.clear();

  for (const auto [entity, world, renderer, pose] : scene.query<scenes::world_transform, scenes::mesh_renderer, scenes::skeleton_pose>(ecs::exclude<scenes::inactive>).each()) {
    if (!renderer.mesh.is_valid() || !pose.skeleton.is_valid()) {
      continue;
    }

    _skinned_entries.push_back(skinned_entry{entity, memory::make_observer(world), memory::make_observer(renderer), memory::make_observer(pose), scene.node_of(entity).try_get_component<scenes::animator>()});
  }

  // Poses are independent per entity, so they evaluate in parallel; the packet appends below stay serial.
  static constexpr auto poses_per_chunk = std::size_t{4u};

  _pose_scratch.resize(std::max(_pose_scratch.size(), (_skinned_entries.size() + poses_per_chunk - 1u) / poses_per_chunk));

  core::engine::get_module<async::async_module>().pool().parallel_for_chunks(0u, _skinned_entries.size(), poses_per_chunk, [&](std::size_t begin, std::size_t end) {
    SBX_PROFILE_SCOPE("Skeleton poses");

    auto& scratch = _pose_scratch[begin / poses_per_chunk];

    for (auto index = begin; index < end; ++index) {
      auto& entry = _skinned_entries[index];

      _evaluate_skeleton_pose(*entry.pose->skeleton, *entry.renderer, entry.animator, *entry.pose, animation_delta_time, scratch);
    }
  });

  for (auto& entry : _skinned_entries) {
    const auto entity = entry.entity;
    const auto& world = *entry.world;
    auto& renderer = *entry.renderer;
    const auto& pose = *entry.pose;

    const auto instance_vertex_count = renderer.mesh->vertex_count();

    if (instance_vertex_count == 0u || !renderer.mesh->has_skin_data() || (skin_scratch_cursor + instance_vertex_count) > skin_scratch_vertex_capacity) {
      continue; // no skin data, or this frame's scratch buffer is full
    }

    const auto joint_offset = static_cast<std::uint32_t>(packet.joint_matrices.size());
    packet.joint_matrices.insert(packet.joint_matrices.end(), pose.skinning_matrices.begin(), pose.skinning_matrices.end());

    const auto output_vertex_address = skin_scratch_address + static_cast<graphics::buffer::address_type>(skin_scratch_cursor) * sizeof(assets::vertex);

    packet.skin_dispatches.push_back(skin_dispatch{
      renderer.mesh->vertex_address(),
      renderer.mesh->skin_vertex_address(),
      output_vertex_address,
      joint_offset,
      instance_vertex_count
    });

    skin_scratch_cursor += instance_vertex_count;

    const auto& submeshes = renderer.mesh->submeshes();
    const auto selected = is_selected(entity);

    for (auto index = std::uint32_t{0u}; index < submeshes.size(); ++index) {
      if (index >= renderer.materials.size()) {
        continue;
      }

      const auto& material = renderer.materials[index];

      if (!material.is_valid()) {
        continue;
      }

      auto command = draw_command{};
      command.mesh = renderer.mesh;
      command.submesh_index = index;
      command.material = material;
      command.instance_count = 1u;
      command.transform_offset = static_cast<std::uint32_t>(packet.transforms.size());
      command.pipeline_id = compute_pipeline_id(*material);
      command.vertex_address_override = output_vertex_address;
      command.resident = assets_module.is_resident(renderer.mesh) && assets_module.is_resident(material);
      command.local_bounds = submeshes[index].bounds.inflated(skinned_bounds_padding_factor);

      packet.transforms.push_back(transform_data{world.matrix, math::matrix4x4::transposed(math::matrix4x4::inverted(world.matrix))});

      // Already single-instance with its own transform, so the outline can share it.
      if (selected) {
        packet.selected_commands.push_back(command);
      }

      if (material->alpha() == assets::alpha_mode::blend) {
        packet.transparent_commands.push_back(command);
      } else {
        if (material->casts_shadow()) {
          packet.shadow_caster_commands.push_back(command);
        }

        packet.opaque_commands.push_back(command);
      }
    }
  }

  auto directional_lights = std::vector<light_data>{};
  auto shadow_caster_found = false;
  auto shadow_caster_index = std::size_t{0u};

  for (auto&& [entity, transform, light] : scene.query<scenes::world_transform, scenes::directional_light>(ecs::exclude<scenes::inactive>).each()) {
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
      packet.shadow_depth_bias = light.shadow_depth_bias;
      packet.shadow_normal_bias = light.shadow_normal_bias;
      packet.shadow_angular_diameter = light.shadow_angular_diameter;
      packet.contact_shadow_length = light.contact_shadow_length;
    }

    directional_lights.push_back(data);
  }

  if (shadow_caster_found && shadow_caster_index != 0u) {
    std::swap(directional_lights.front(), directional_lights[shadow_caster_index]);
  }

  packet.lights.insert(packet.lights.end(), directional_lights.begin(), directional_lights.end());

  packet.directional_light_count = static_cast<std::uint32_t>(packet.lights.size());

  // The exact query _select_local_shadows runs, so the iteration order matches _point_shadow_indices.
  auto point_index = std::size_t{0u};

  for (auto&& [entity, transform, light, id] : scene.query<scenes::world_transform, scenes::point_light, scenes::id>(ecs::exclude<scenes::inactive>).each()) {
    const auto& matrix = transform.matrix;
    auto& out = packet.lights.emplace_back();

    out.shadow_index = point_index < _point_shadow_indices.size() ? _point_shadow_indices[point_index] : 0xFFFFFFFFu;
    ++point_index;

    out.type = light_type::point;
    out.color = math::vector4{light.color.r(), light.color.g(), light.color.b(), light.intensity};
    out.position = math::vector4{matrix[3].x(), matrix[3].y(), matrix[3].z(), light.range};
  }

  // The exact query _select_local_shadows runs, so the iteration order matches _spot_shadow_indices.
  auto spot_index = std::size_t{0u};

  for (auto&& [entity, transform, light, id] : scene.query<scenes::world_transform, scenes::spot_light, scenes::id>(ecs::exclude<scenes::inactive>).each()) {
    const auto& matrix = transform.matrix;
    auto& out = packet.lights.emplace_back();

    out.shadow_index = spot_index < _spot_shadow_indices.size() ? _spot_shadow_indices[spot_index] : 0xFFFFFFFFu;
    ++spot_index;

    out.type = light_type::spot;
    out.color = math::vector4{light.color.r(), light.color.g(), light.color.b(), light.intensity};
    out.position = math::vector4{math::vector3{matrix[3]}, light.range};
    out.direction = math::vector4{math::vector3::normalized(math::vector3{-matrix[2].x(), -matrix[2].y(), -matrix[2].z()}), 0.0f};
    out.inner_cos = std::cos(light.inner_angle);
    out.outer_cos = std::cos(light.outer_angle);
  }

  _gather_decals(packet);
  _gather_reflection_probes(packet);

  const auto delta_time = scenes_module.simulation_delta_time().value();
  const auto time = scenes_module.simulation_time().value();

  packet.delta_time = delta_time;
  packet.time = time;

  auto billboard_buckets = std::unordered_map<std::pair<std::uint32_t, assets::emitter_blend_mode>, std::vector<particle_billboard_instance>, billboard_key_hash>{};

  auto mesh_buckets = std::unordered_map<particle_mesh_key, particle_mesh_bucket, particle_mesh_key_hash>{};
  auto trail_buckets = std::unordered_map<assets::emitter_blend_mode, std::vector<trail_vertex>>{};

  for (auto&& [entity, world, instance] : scene.query<scenes::world_transform, scenes::particle_effect>(ecs::exclude<scenes::inactive>).each()) {
    if (!instance.effect.is_valid()) {
      continue;
    }

    const auto& configs = instance.effect->emitters();
    const auto emitter_count = std::min(configs.size(), instance.emitters.size());

    for (auto index = std::size_t{0u}; index < emitter_count; ++index) {
      const auto& config = configs[index];

      if (config.simulation_mode == assets::particle_simulation_mode::gpu) {
        _extract_gpu_particle_emitter(packet, config, instance.emitters[index], instance, world.matrix, delta_time);
        continue;
      }

      const auto& runtime = instance.emitters[index];

      if (config.trail.enabled && !runtime.trails.empty()) {
        auto& bucket = trail_buckets[config.blend_mode];

        for (const auto& trail : runtime.trails) {
          const auto point_count = trail.points.size();

          if (point_count < 2u) {
            continue;
          }

          auto left = std::array<trail_vertex, assets::trail_max_points>{};
          auto right = std::array<trail_vertex, assets::trail_max_points>{};

          for (auto i = std::size_t{0u}; i < point_count; ++i) {
            const auto& point = trail.points[i];

            const auto ribbon_t = 1.0f - static_cast<std::float_t>(i) / static_cast<std::float_t>(point_count - 1u);

            auto color = config.trail.color_over_trail.has_keys() ? config.trail.color_over_trail.evaluate(ribbon_t) : point.color;

            const auto fade = config.trail.lifetime > 0.0f ? std::clamp(1.0f - point.age / config.trail.lifetime, 0.0f, 1.0f) : 1.0f;
            color.a() *= fade;

            const auto direction = (i + 1u < point_count) ? math::vector3::normalized(trail.points[i + 1u].position - point.position) : math::vector3::normalized(point.position - trail.points[i - 1u].position);

            const auto view_direction = math::vector3::normalized(packet.camera.position - point.position);

            auto side = math::vector3::cross(direction, view_direction);
            const auto side_length = side.length();
            side = (side_length > 1e-5f) ? side * (config.trail.width * 0.5f / side_length) : math::vector3{0.0f, 0.0f, 0.0f};

            left[i] = trail_vertex{point.position - side, color};
            right[i] = trail_vertex{point.position + side, color};
          }

          for (auto i = std::size_t{0u}; i + 1u < point_count; ++i) {
            bucket.push_back(left[i]);
            bucket.push_back(right[i]);
            bucket.push_back(left[i + 1u]);

            bucket.push_back(right[i]);
            bucket.push_back(right[i + 1u]);
            bucket.push_back(left[i + 1u]);
          }
        }
      }

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
            const auto model =
              math::matrix4x4::translated(math::matrix4x4::identity, particle.position) *
              math::matrix4x4::rotated(math::matrix4x4::identity, math::vector3{0.0f, 1.0f, 0.0f}, math::radian{particle.rotation}) *
              math::matrix4x4::scaled(math::matrix4x4::identity, math::vector3{particle.size, particle.size, particle.size});

            bucket.instances.push_back(particle_mesh_instance{.model = model, .color = particle.color});
          }
        }

        continue;
      }

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

  auto ordered_mesh_buckets = std::vector<std::pair<particle_mesh_key, particle_mesh_bucket>>{std::make_move_iterator(mesh_buckets.begin()), std::make_move_iterator(mesh_buckets.end())};
  std::ranges::sort(ordered_mesh_buckets, [](const particle_mesh_key& lhs, const particle_mesh_key& rhs) { return lhs < rhs; }, &std::pair<particle_mesh_key, particle_mesh_bucket>::first);

  for (auto& [key, bucket] : ordered_mesh_buckets) {
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

  for (auto& [blend_mode, vertices] : trail_buckets) {
    if (vertices.empty()) {
      continue;
    }

    packet.trail_commands.push_back(particle_trail_command{
      .blend_mode = blend_mode,
      .vertex_count = static_cast<std::uint32_t>(vertices.size()),
      .vertex_offset = static_cast<std::uint32_t>(packet.trail_vertices.size())
    });

    packet.trail_vertices.insert(packet.trail_vertices.end(), vertices.begin(), vertices.end());
  }

  _particle_pool_additive->tick(delta_time);
  _particle_pool_alpha_blend->tick(delta_time);

  // Every list is complete: lay out each one's culled output per view.
  const auto [opaque_args, opaque_transforms] = assign_lod_layout(packet.opaque_commands);
  const auto [shadow_args, shadow_transforms] = assign_lod_layout(packet.shadow_caster_commands);

  packet.cull_args_stride = std::max(opaque_args, shadow_args);
  packet.cull_transform_stride = std::max(opaque_transforms, shadow_transforms);

  // Every camera instance gets its own visibility flag, so no two culls race on one.
  for (auto& command : packet.opaque_commands) {
    command.occlusion_offset = packet.occlusion_instance_count;
    packet.occlusion_instance_count += command.instance_count;
  }

  // Needs every shadow caster command, so it runs last.
  _update_local_shadow_cache(packet);

  // A capture face sees the scene as the game does.
  if (_capture_face) {
    packet.selected_commands.clear();
  }

  return packet;
}

auto scene_renderer_module::_requested_sample_count() const -> graphics::samples {
  const auto requested = core::engine::has_project() ? core::engine::project().msaa_samples() : core::project::default_msaa_samples;

  const auto& limits = core::engine::get_module<graphics::graphics_module>().physical_device().properties().limits;

  return graphics::clamp_sample_count(requested, limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts);
}

auto scene_renderer_module::_build_graph() -> void {
  _graph.clear();

  _graph.add_pass<skin_pass>();
  _graph.add_pass<frustum_cull_pass>();
  _graph.add_pass<depth_pre_pass>(_sample_count);
  _graph.add_pass<hiz_pass>();
  _graph.add_pass<frustum_cull_pass>(true);
  _graph.add_pass<depth_pre_pass>(_sample_count, true);
  _graph.add_pass<ambient_occlusion_pass>();
  _graph.add_pass<light_culling_pass>();
  _graph.add_pass<shadow_pass>();
  _graph.add_pass<local_shadow_pass>();
  _graph.add_pass<opaque_pass>(_sample_count);
  _graph.add_pass<skybox_pass>(_sample_count);
  _graph.add_pass<grid_pass>(_sample_count);
  _graph.add_pass<debug_draw_pass>(_sample_count);
  _graph.add_pass<transparent_accumulate_pass>(_sample_count);
  _graph.add_pass<particle_simulate_pass>(*_particle_pool_additive, *_particle_pool_alpha_blend);
  _graph.add_pass<particle_pass>(_sample_count);
  _graph.add_pass<canvas_world_pass>(_sample_count);
  // After every MSAA pass: they resolve color_msaa over color, which would erase the transparents this composites into color.
  _graph.add_pass<transparent_resolve_pass>();
  _graph.add_pass<bloom_pass>();
  _graph.add_pass<tonemap_pass>();
  _graph.add_pass<fxaa_pass>();
  _graph.add_pass<selection_outline_pass>();
  _graph.add_pass<canvas_pass>();

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  _graph.initialize_gpu_queries(graphics_module.physical_device(), graphics_module.logical_device());
}

auto scene_renderer_module::_apply_sample_count() -> void {
  const auto requested = _requested_sample_count();

  if (requested == _sample_count) {
    return;
  }

  // Passes own per-frame state the in-flight frames still use.
  core::engine::get_module<graphics::graphics_module>().logical_device().wait_idle();

  _sample_count = requested;

  // The old passes retire their own per-frame buffers when the graph drops them.
  _build_graph();
}

auto scene_renderer_module::record(graphics::command_buffer& command_buffer, math::vector2u extent) -> void {
  SBX_PROFILE_SCOPE("scene_renderer_module::record");
  SBX_PROFILE_GPU_SCOPE(command_buffer, "scene_renderer_module::record");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& frame_context = graphics_module.frame_context();
  auto& upload_context = graphics_module.upload_context();

  const auto& packet = _work_packet;

  const auto viewport_extent = (_viewport_extent.x() > 0u && _viewport_extent.y() > 0u) ? _viewport_extent : extent;
  const auto scene_extent = _capture_face ? math::vector2u{_capture_jobs.front().resolution, _capture_jobs.front().resolution} : viewport_extent;

  {
    SBX_STATS_SCOPE("AssetUpdate");
    assets_module.process_uploads(frame_context.frame_index());
  }

  {
    SBX_PROFILE_SCOPE("upload_context::flush");
    upload_context.flush(command_buffer, frame_context.frame_index());
  }

  auto irradiance_index = 0xFFFFFFFFu;
  auto prefiltered_index = 0xFFFFFFFFu;
  auto prefiltered_mip_count = 0u;

  // Reflection probes need the lookup table even without a sky.
  const auto brdf_lut_index = assets_module.brdf_lut_index();

  if (packet.camera.is_active && packet.environment.is_valid() && assets_module.is_resident(packet.environment)) {
    irradiance_index = packet.environment->irradiance_index();
    prefiltered_index = packet.environment->prefiltered_index();
    prefiltered_mip_count = packet.environment->prefiltered_mip_count();
  }

  {
    SBX_PROFILE_SCOPE("scene_renderer_module::resize_targets");
    _apply_sample_count();
    _resize_targets(scene_extent);
    _ensure_frame_capacity(packet);
  }

  _has_rendered = packet.camera.is_active;
  _has_rendered_shadows = false;

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
    .scene_depth = _scene_depth_image,
    .scene_depth_index = _scene_depth_index,
    .ambient_occlusion_raw = _ambient_occlusion_raw_image,
    .ambient_occlusion = _ambient_occlusion_image,
    .ambient_occlusion_raw_index = _ambient_occlusion_raw_index,
    .ambient_occlusion_index = _ambient_occlusion_index,
    .ambient_occlusion_raw_storage_index = _ambient_occlusion_raw_storage_index,
    .ambient_occlusion_storage_index = _ambient_occlusion_storage_index,
    .color = _color_image,
    .color_msaa = _color_msaa_image.is_valid() ? _color_msaa_image : _color_image,
    .color_index = _color_index,
    .ldr_index = _ldr_index,
    .final_image = _final_image,
    .final_image_index = _final_image_index,
    .accumulator = _accum_image,
    .accumulator_msaa = _accumulator_msaa_image.is_valid() ? _accumulator_msaa_image : _accum_image,
    .accumulator_index = _accumulator_index,
    .revealage = _revealage_image,
    .revealage_msaa = _revealage_msaa_image.is_valid() ? _revealage_msaa_image : _revealage_image,
    .revealage_index = _revealage_index,
    .bloom_upsample = _bloom_upsample_image,
    .bloom_upsample_index = _bloom_upsample_index,
    .selection_mask_index = _selection_mask_index,
    .jump_flood_indices = _jump_flood_indices
  };

  {
    SBX_PROFILE_SCOPE("scene_renderer_module::prepare_frame");
    _prepare_frame(context);
  }

  {
    SBX_PROFILE_SCOPE("render_graph::execute");
    _graph.execute(context);
  }

  if (_capture_face) {
    _record_capture_copy(command_buffer);
  }

  // The culled args and the occlusion counter are read on the host once this slot's fence signals (_prepare_frame); make the GPU's writes visible to it.
  {
    auto barrier = VkMemoryBarrier2{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
    command_buffer.memory_dependency(barrier);
  }

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

  _shadow_sampler_index = bindless_table.sampler_index(graphics::sampler::create_info{
    .mipmap_mode = graphics::mipmap_mode::nearest,
    .address_mode_u = graphics::address_mode::clamp_to_edge,
    .address_mode_v = graphics::address_mode::clamp_to_edge,
    .address_mode_w = graphics::address_mode::clamp_to_edge,
    .max_lod = 0.0f,
    .compare = graphics::compare_operation::less_or_equal,
    .name = "Shadow Comparison Sampler"
  });

  _color_index = bindless_table.reserve_sampled_image();

  _final_image_index = bindless_table.reserve_sampled_image();

  _ldr_index = bindless_table.reserve_sampled_image();

  _accumulator_index = bindless_table.reserve_sampled_image();

  _revealage_index = bindless_table.reserve_sampled_image();

  _bloom_upsample_index = bindless_table.reserve_sampled_image();

  _scene_depth_index = bindless_table.reserve_sampled_image();

  _ambient_occlusion_raw_index = bindless_table.reserve_sampled_image();

  _ambient_occlusion_index = bindless_table.reserve_sampled_image();

  _selection_mask_index = bindless_table.reserve_sampled_image();

  for (auto& index : _jump_flood_indices) {
    index = bindless_table.reserve_sampled_image();
  }

  _frame_buffer =registry.emplace<graphics::buffer>(graphics::buffer::create_info{
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

  create_slotted_buffer(_transform_buffer, _transform_addresses, sizeof(transform_data), _transform_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::host_write, "Instance Transforms");
  create_slotted_buffer(_culled_indirect_args_buffer, _culled_indirect_args_addresses, sizeof(VkDrawIndexedIndirectCommand), _culled_indirect_args_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage | graphics::buffer_usage::indirect, graphics::memory_usage::host_write, "Culled Draw Args");
  create_slotted_buffer(_culled_transform_buffer, _culled_transform_addresses, sizeof(transform_data), _culled_transform_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::device_local, "Culled Instance Transforms");
  create_slotted_buffer(_instanced_culled_buffer, _instanced_culled_addresses, sizeof(transform_data), _instanced_culled_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::device_local, "Culled Instanced Transforms");
  create_slotted_buffer(_local_shadow_view_buffer, _local_shadow_view_addresses, sizeof(local_shadow_view), _local_shadow_view_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::host_write, "Local Shadow Views");
  create_slotted_buffer(_decal_buffer, _decal_addresses, sizeof(decal_data), _decal_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::host_write, "Decals");
  create_slotted_buffer(_probe_buffer, _probe_addresses, sizeof(probe_data), _probe_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::host_write, "Reflection Probes");

  _joint_palette_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(math::matrix4x4) * joint_palette_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Skin Joint Palette"
  });

  const auto joint_palette_base = registry.get<graphics::buffer>(_joint_palette_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _joint_palette_addresses[slot] = joint_palette_base + slot * joint_palette_capacity * sizeof(math::matrix4x4);
  }

  _skin_scratch_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(assets::vertex) * skin_scratch_vertex_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Skin Scratch Vertices"
  });

  const auto skin_scratch_base = registry.get<graphics::buffer>(_skin_scratch_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _skin_scratch_addresses[slot] = skin_scratch_base + slot * skin_scratch_vertex_capacity * sizeof(assets::vertex);
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
    .size = memory::stride_v<std::uint32_t> * cluster_index_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Cluster Light Indices"
  });

  const auto cluster_light_index_base = registry.get<graphics::buffer>(_cluster_light_index_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _cluster_light_index_addresses[slot] = cluster_light_index_base + slot * cluster_index_capacity * memory::stride_v<std::uint32_t>;
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

    const auto grayscale = VkComponentMapping{VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE};
    _shadow_map_preview_views[cascade] = registry.get<graphics::image>(_shadow_map_images[cascade]).create_view(graphics::image_view_type::two_dimensional, 0u, 1u, 0u, 1u, grayscale);
  }

  _local_shadow_atlas_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{local_shadow_atlas_resolution, local_shadow_atlas_resolution, 1u},
    .format = graphics::format::d32_sfloat,
    .usage = graphics::image_usage::depth_stencil_attachment | graphics::image_usage::sampled,
    .samples = graphics::samples::count_1,
    .name = "Local Shadow Atlas"
  });

  _local_shadow_atlas_index = bindless_table.reserve_sampled_image();
  bindless_table.write_sampled_image(_local_shadow_atlas_index, registry.get<graphics::image>(_local_shadow_atlas_image).view());

  const auto& atlas = registry.get<graphics::image>(_local_shadow_atlas_image);

  const auto grayscale = VkComponentMapping{VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE};
  _local_shadow_atlas_preview_view = atlas.create_view(graphics::image_view_type::two_dimensional, 0u, 1u, 0u, 1u, grayscale);

  // local_shadow_pass keeps the atlas between frames (preserve_contents), so the graph's first barrier expects the layout a frame ends in.
  // No tile is cached yet, so the undefined contents are never sampled before being drawn.
  auto commands = graphics::command_buffer{graphics::queue::type::graphics, true};

  auto to_read = graphics::command_buffer::image_transition_data{};
  to_read.image = atlas.handle();
  to_read.src_stage_mask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  to_read.src_access_mask = 0u;
  to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
  to_read.old_layout = graphics::image_layout::undefined;
  to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
  to_read.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
  commands.transition_image_layout(to_read);

  commands.submit_idle();

  // Occlusion culling: a fixed-size farthest-depth pyramid, sampled as a whole and written level by level.
  _hiz_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{hiz_width, hiz_height, 1u},
    .format = graphics::format::r32_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .mip_levels = hiz_mip_count,
    .samples = graphics::samples::count_1,
    .name = "HiZ"
  });

  const auto& hiz = registry.get<graphics::image>(_hiz_image);

  _hiz_index = bindless_table.reserve_sampled_image();
  bindless_table.write_sampled_image(_hiz_index, hiz.view());

  for (auto level = std::uint32_t{0u}; level < hiz_mip_count; ++level) {
    _hiz_mip_views[level] = hiz.create_view(graphics::image_view_type::two_dimensional, level, 1u, 0u, 1u);
    _hiz_storage_indices[level] = bindless_table.register_storage_image(_hiz_mip_views[level]);
  }

  _create_occlusion_visibility_buffer();

  _occlusion_counter_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<std::uint32_t> * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_read,
    .name = "Occlusion Culled Counter"
  });

  const auto occlusion_counter_base = registry.get<graphics::buffer>(_occlusion_counter_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _occlusion_counter_addresses[slot] = occlusion_counter_base + slot * memory::stride_v<std::uint32_t>;
  }
}

auto scene_renderer_module::_create_occlusion_visibility_buffer() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  if (_occlusion_visibility_buffer.is_valid()) {
    registry.retire(_occlusion_visibility_buffer, graphics_module.frame_context().frame_index());
  }

  // One region shared by every frame: each frame's early cull reads what the previous frame's late cull wrote, and the graph orders them across frames.
  // A fresh buffer's flags are arbitrary, which only moves instances between the early and late draws for a frame.
  _occlusion_visibility_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<std::uint32_t> * _occlusion_visibility_capacity,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Occlusion Visibility"
  });

  _occlusion_visibility_address = registry.get<graphics::buffer>(_occlusion_visibility_buffer).address();
}

auto scene_renderer_module::_resize_targets(const math::vector2u extent) -> void {
  if (_target_extent == extent && _target_sample_count == _sample_count) {
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
    registry.retire(_scene_depth_image, frame_index);
    registry.retire(_color_image, frame_index);
    registry.retire(_final_image, frame_index);
    registry.retire(_ldr_image, frame_index);
    registry.retire(_accum_image, frame_index);
    registry.retire(_revealage_image, frame_index);

    for (auto image : {_color_msaa_image, _accumulator_msaa_image, _revealage_msaa_image}) {
      if (image.is_valid()) {
        registry.retire(image, frame_index);
      }
    }
    registry.retire(_bloom_downsample_image, frame_index);
    registry.retire(_bloom_upsample_image, frame_index);
    registry.retire(_ambient_occlusion_raw_image, frame_index);
    registry.retire(_ambient_occlusion_image, frame_index);
    registry.retire(_selection_mask_image, frame_index);
    registry.retire(_selection_depth_image, frame_index);

    for (auto image : _jump_flood_images) {
      registry.retire(image, frame_index);
    }

    // Fresh indices: in-flight frames still sample the old images through the old ones.
    for (auto index : {std::ref(_scene_depth_index), std::ref(_color_index), std::ref(_accumulator_index), std::ref(_revealage_index), std::ref(_bloom_upsample_index), std::ref(_final_image_index), std::ref(_ldr_index), std::ref(_ambient_occlusion_raw_index), std::ref(_ambient_occlusion_index), std::ref(_selection_mask_index), std::ref(_jump_flood_indices[0]), std::ref(_jump_flood_indices[1])}) {
      bindless_table.unregister_sampled_image(index.get());
      index.get() = bindless_table.reserve_sampled_image();
    }

    for (auto index : {_ambient_occlusion_raw_storage_index, _ambient_occlusion_storage_index}) {
      if (index != 0xFFFFFFFFu) {
        bindless_table.unregister_storage_image(index);
      }
    }
  }

  _depth_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::d32_sfloat,
    .usage = graphics::image_usage::depth_stencil_attachment | graphics::image_usage::transfer_source,
    .samples = _sample_count,
    .name = "Depth"
  });

  _scene_depth_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::d32_sfloat,
    .usage = graphics::image_usage::depth_stencil_attachment | graphics::image_usage::sampled | graphics::image_usage::transfer_destination,
    .samples = graphics::samples::count_1,
    .name = "Scene Depth"
  });

  bindless_table.write_sampled_image(_scene_depth_index, registry.get<graphics::image>(_scene_depth_image).view());

  // Ambient occlusion: half resolution RGBA8 (r used), the format bindless float4 storage images write.
  const auto ambient_occlusion_extent = math::vector2u{std::max(extent.x() / 2u, 1u), std::max(extent.y() / 2u, 1u)};

  for (auto [image, index, storage_index, name] : {
    std::tuple{&_ambient_occlusion_raw_image, &_ambient_occlusion_raw_index, &_ambient_occlusion_raw_storage_index, "Ambient Occlusion Raw"},
    std::tuple{&_ambient_occlusion_image, &_ambient_occlusion_index, &_ambient_occlusion_storage_index, "Ambient Occlusion"}
  }) {
    *image = registry.emplace<graphics::image>(graphics::image::create_info{
      .extent = math::vector3u{ambient_occlusion_extent, 1u},
      .format = graphics::format::r8g8b8a8_unorm,
      .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
      .samples = graphics::samples::count_1,
      .name = name
    });

    const auto view = registry.get<graphics::image>(*image).view();
    bindless_table.write_sampled_image(*index, view);
    *storage_index = bindless_table.register_storage_image(view);
  }

  // Without MSAA, passes render straight into the single-sample images (see _build_graph_resources()).
  const auto has_msaa = _sample_count != graphics::samples::count_1;

  _color_msaa_image = !has_msaa ? graphics::image_handle{} : registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::color_attachment,
    .samples = _sample_count,
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

  _accumulator_msaa_image = !has_msaa ? graphics::image_handle{} : registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::color_attachment,
    .samples = _sample_count,
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

  _revealage_msaa_image = !has_msaa ? graphics::image_handle{} : registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16_sfloat,
    .usage = graphics::image_usage::color_attachment,
    .samples = _sample_count,
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

  const auto bloom_extent = bloom_pass::extent_for(extent);
  const auto bloom_mip_count = bloom_pass::mip_count_for(extent);

  _bloom_downsample_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{bloom_extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .mip_levels = bloom_mip_count,
    .samples = graphics::samples::count_1,
    .name = "Bloom Downsample"
  });

  _bloom_upsample_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{bloom_extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .mip_levels = bloom_mip_count - 1u,
    .samples = graphics::samples::count_1,
    .name = "Bloom Upsample"
  });

  bindless_table.write_sampled_image(_bloom_upsample_index, registry.get<graphics::image>(_bloom_upsample_image).view());

  const auto final_image_format = static_cast<graphics::format>(surface.format().format);

  _final_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = final_image_format,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .samples = graphics::samples::count_1,
    .name = "Final Viewport Image"
  });

  bindless_table.write_sampled_image(_final_image_index, registry.get<graphics::image>(_final_image).view());

  _ldr_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = final_image_format,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .samples = graphics::samples::count_1,
    .name = "LDR Image"
  });

  bindless_table.write_sampled_image(_ldr_index, registry.get<graphics::image>(_ldr_image).view());

  // Selection outline: only alpha > 0.5 of the mask matters; the jump flood targets hold (offset.xy, distance², side), so full float.
  _selection_mask_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r8g8b8a8_unorm,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .samples = graphics::samples::count_1,
    .name = "Selection Mask"
  });

  bindless_table.write_sampled_image(_selection_mask_index, registry.get<graphics::image>(_selection_mask_image).view());

  _selection_depth_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::d32_sfloat,
    .usage = graphics::image_usage::depth_stencil_attachment,
    .samples = graphics::samples::count_1,
    .name = "Selection Depth"
  });

  for (auto i = std::size_t{0u}; i < _jump_flood_images.size(); ++i) {
    _jump_flood_images[i] = registry.emplace<graphics::image>(graphics::image::create_info{
      .extent = math::vector3u{extent, 1u},
      .format = graphics::format::r32g32b32a32_sfloat,
      .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
      .samples = graphics::samples::count_1,
      .name = "Jump Flood " + std::to_string(i)
    });

    bindless_table.write_sampled_image(_jump_flood_indices[i], registry.get<graphics::image>(_jump_flood_images[i]).view());
  }

  _target_extent = extent;
  _target_sample_count = _sample_count;

  // This frame's passes already use the new indices, so flush now instead of at the next begin_frame.
  bindless_table.flush_writes();

  _graph.compile(_build_graph_resources());
}

auto scene_renderer_module::_select_local_shadows(scenes::scene& scene, render_packet& packet) -> void {
  // The PCF footprint plus one texel of filtering, kept inside every tile by widening its fov.
  static constexpr auto filter_margin_texels = 4.0f;

  // A point light's cube faces, in the order shadows/local.slang picks them by major axis: +X, -X, +Y, -Y, +Z, -Z.
  static constexpr auto face_directions = std::array<std::array<std::float_t, 3u>, 6u>{{{1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}};
  static constexpr auto face_ups = std::array<std::array<std::float_t, 3u>, 6u>{{{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};

  struct candidate {
    bool is_point;
    std::size_t light; // index in its own query order (_spot_shadow_indices or _point_shadow_indices)
    std::uint64_t identity; // the node's id: ties in tile size place lights in the same spot every frame, so their cached tiles stay valid
    std::float_t score;
    std::uint32_t resolution;
    std::uint32_t tile_count;
    math::vector3 position;
    math::vector3 direction;
    std::float_t outer_angle;
    std::float_t range;
    std::float_t depth_bias;
    std::float_t normal_bias;
  }; // struct candidate

  _spot_shadow_indices.clear();
  _point_shadow_indices.clear();
  _local_shadow_bounds.clear();

  const auto& camera = packet.camera;

  const auto aspect = (_viewport_extent.x() > 0u && _viewport_extent.y() > 0u) ? static_cast<std::float_t>(_viewport_extent.x()) / static_cast<std::float_t>(_viewport_extent.y()) : 16.0f / 9.0f;
  const auto camera_projection = math::matrix4x4::perspective(math::degree{camera.fov_degrees}, aspect, camera.near_plane, camera.far_plane);
  const auto planes = math::extract_frustum_planes(camera_projection * camera.view);

  const auto touches_view = [&](const math::vector3& position, std::float_t range) {
    return std::ranges::all_of(planes, [&](const auto& plane) {
      return plane.x() * position.x() + plane.y() * position.y() + plane.z() * position.z() + plane.w() >= -range;
    });
  };

  const auto score_of = [&](std::float_t intensity, std::float_t range, const math::vector3& position) {
    return intensity * range / std::max((position - camera.position).length(), 1.0f);
  };

  auto candidates = std::vector<candidate>{};

  for (auto&& [entity, transform, light, id] : scene.query<scenes::world_transform, scenes::spot_light, scenes::id>(ecs::exclude<scenes::inactive>).each()) {
    const auto light_index = _spot_shadow_indices.size();
    _spot_shadow_indices.push_back(0xFFFFFFFFu);

    const auto position = math::vector3{transform.matrix[3]};

    if (!camera.is_active || !light.casts_shadows || light.range <= 0.0f || light.outer_angle <= 0.0f || !touches_view(position, light.range)) {
      continue;
    }

    const auto& matrix = transform.matrix;

    candidates.push_back(candidate{
      .is_point = false,
      .light = light_index,
      .identity = id.value(),
      .score = score_of(light.intensity, light.range, position),
      .resolution = std::bit_ceil(std::clamp(light.shadow_resolution, 256u, 2048u)),
      .tile_count = 1u,
      .position = position,
      .direction = math::vector3::normalized(math::vector3{-matrix[2].x(), -matrix[2].y(), -matrix[2].z()}),
      .outer_angle = light.outer_angle,
      .range = light.range,
      .depth_bias = light.shadow_depth_bias,
      .normal_bias = light.shadow_normal_bias
    });
  }

  for (auto&& [entity, transform, light, id] : scene.query<scenes::world_transform, scenes::point_light, scenes::id>(ecs::exclude<scenes::inactive>).each()) {
    const auto light_index = _point_shadow_indices.size();
    _point_shadow_indices.push_back(0xFFFFFFFFu);

    const auto position = math::vector3{transform.matrix[3]};

    if (!camera.is_active || !light.casts_shadows || light.range <= 0.0f || !touches_view(position, light.range)) {
      continue;
    }

    candidates.push_back(candidate{
      .is_point = true,
      .light = light_index,
      .identity = id.value(),
      .score = score_of(light.intensity, light.range, position),
      .resolution = std::bit_ceil(std::clamp(light.shadow_resolution, 256u, 2048u)),
      .tile_count = 6u,
      .position = position,
      .direction = math::vector3{0.0f, 0.0f, -1.0f},
      .outer_angle = 0.0f,
      .range = light.range,
      .depth_bias = light.shadow_depth_bias,
      .normal_bias = light.shadow_normal_bias
    });
  }

  std::ranges::stable_sort(candidates, std::ranges::greater{}, &candidate::score);

  // The most important lights first, while their tiles' total area still fits.
  auto selected = std::vector<candidate>{};
  auto used_area = std::uint64_t{0u};
  const auto atlas_area = std::uint64_t{local_shadow_atlas_resolution} * local_shadow_atlas_resolution;

  for (const auto& entry : candidates) {
    const auto area = std::uint64_t{entry.tile_count} * entry.resolution * entry.resolution;

    if (used_area + area <= atlas_area) {
      used_area += area;
      selected.push_back(entry);
    }
  }

  // Shelf packing, largest first: power-of-two squares sorted this way fill the power-of-two atlas without gaps.
  // Placement ignores score, so a set of lights that doesn't change keeps its tiles where they were.
  std::ranges::sort(selected, [](const candidate& lhs, const candidate& rhs) {
    return lhs.resolution != rhs.resolution ? lhs.resolution > rhs.resolution : lhs.identity < rhs.identity;
  });

  const auto atlas_size = static_cast<std::float_t>(local_shadow_atlas_resolution);

  auto x = 0u;
  auto y = 0u;
  auto shelf_height = 0u;

  // Where the next tile of a given size goes, advancing the shelf cursor; nullopt once the atlas is full.
  const auto allocate = [&](std::uint32_t size, std::uint32_t& cursor_x, std::uint32_t& cursor_y, std::uint32_t& cursor_shelf) -> std::optional<math::vector2u> {
    if (cursor_x + size > local_shadow_atlas_resolution) {
      cursor_x = 0u;
      cursor_y += cursor_shelf;
      cursor_shelf = 0u;
    }

    if (cursor_y + size > local_shadow_atlas_resolution) {
      return std::nullopt;
    }

    const auto origin = math::vector2u{cursor_x, cursor_y};

    cursor_x += size;
    cursor_shelf = std::max(cursor_shelf, size);

    return origin;
  };

  for (const auto& entry : selected) {
    // All of a light's tiles or none: a point light needs every face.
    auto trial_x = x;
    auto trial_y = y;
    auto trial_shelf = shelf_height;
    auto origins = std::vector<math::vector2u>{};

    for (auto tile = 0u; tile < entry.tile_count; ++tile) {
      if (const auto origin = allocate(entry.resolution, trial_x, trial_y, trial_shelf)) {
        origins.push_back(*origin);
      }
    }

    if (origins.size() != entry.tile_count) {
      continue;
    }

    x = trial_x;
    y = trial_y;
    shelf_height = trial_shelf;

    const auto resolution = static_cast<std::float_t>(entry.resolution);
    const auto first_view = static_cast<std::uint32_t>(packet.local_shadow_views.size());

    (entry.is_point ? _point_shadow_indices : _spot_shadow_indices)[entry.light] = first_view;

    // Widened so a PCF kernel at the edge of the cone or cube face still samples inside the tile.
    const auto base_half_fov = entry.is_point ? math::pif * 0.25f : std::min(entry.outer_angle, 1.5f);
    const auto half_fov = std::atan(std::tan(base_half_fov) * (1.0f + 2.0f * filter_margin_texels / resolution));
    const auto near_plane = std::max(0.05f, entry.range * 0.01f);
    const auto projection = math::matrix4x4::perspective(math::angle{math::radian{2.0f * half_fov}}, 1.0f, near_plane, entry.range);

    for (auto tile = 0u; tile < entry.tile_count; ++tile) {
      auto direction = entry.direction;
      auto up = std::abs(direction.y()) > 0.99f ? math::vector3{1.0f, 0.0f, 0.0f} : math::vector3{0.0f, 1.0f, 0.0f};

      if (entry.is_point) {
        direction = math::vector3{face_directions[tile][0], face_directions[tile][1], face_directions[tile][2]};
        up = math::vector3{face_ups[tile][0], face_ups[tile][1], face_ups[tile][2]};
      }

      const auto view = math::matrix4x4::look_at(entry.position, entry.position + direction, up);
      const auto& origin = origins[tile];

      packet.local_shadow_views.push_back(local_shadow_view{
        .view_projection = projection * view,
        .atlas_rect = math::vector4{static_cast<std::float_t>(origin.x()) / atlas_size, static_cast<std::float_t>(origin.y()) / atlas_size, resolution / atlas_size, resolution / atlas_size},
        .bias = math::vector4{entry.depth_bias, entry.normal_bias, 2.0f * std::tan(half_fov) / resolution, 0.0f}
      });

      _local_shadow_bounds.push_back(math::vector4{entry.position, entry.range});
    }
  }
}

auto scene_renderer_module::_update_local_shadow_cache(render_packet& packet) -> void {
  const auto& views = packet.local_shadow_views;

  packet.local_shadow_dirty.assign(views.size(), 0u);

  auto current_keys = std::unordered_set<std::uint64_t>{};
  current_keys.reserve(views.size());

  // The casters a light's sphere reaches, hashed once per light (a point light's faces share it); a skinned caster changes every frame.
  auto content = std::size_t{0u};
  auto is_dynamic = false;
  auto content_bounds = std::optional<math::vector4>{};

  const auto hash_matrix = [](std::size_t& seed, const math::matrix4x4& matrix) {
    for (auto column = 0u; column < 4u; ++column) {
      for (auto row = 0u; row < 4u; ++row) {
        utility::hash_combine(seed, matrix[column][row]);
      }
    }
  };

  for (auto tile = std::size_t{0u}; tile < views.size(); ++tile) {
    const auto& bounds = _local_shadow_bounds[tile];

    if (content_bounds != bounds) {
      content_bounds = bounds;
      content = 0u;
      is_dynamic = false;

      const auto light_position = math::vector3{bounds.x(), bounds.y(), bounds.z()};

      for (const auto& command : packet.shadow_caster_commands) {
        const auto mesh_generation = command.mesh.is_valid() ? command.mesh.generation() : 0u;
        const auto material_generation = command.material.is_valid() ? command.material.generation() : 0u;

        // Instanced draws have no per-instance bounds here, so they count for every light.
        if (command.instances) {
          utility::hash_combine(content, reinterpret_cast<std::uintptr_t>(command.instances.get()), mesh_generation, material_generation, command.submesh_index, command.resident);

          if (command.transform_offset < packet.transforms.size()) {
            hash_matrix(content, packet.transforms[command.transform_offset].model);
          }

          continue;
        }

        const auto local_center = command.local_bounds.center();
        const auto local_radius = ((command.local_bounds.max() - command.local_bounds.min()) * 0.5f).length();

        for (auto instance = std::uint32_t{0u}; instance < command.instance_count; ++instance) {
          const auto index = static_cast<std::size_t>(command.transform_offset) + instance;

          if (index >= packet.transforms.size()) {
            break;
          }

          const auto& model = packet.transforms[index].model;
          const auto world_center = model * math::vector4{local_center, 1.0f};
          const auto scale = std::max({math::vector3{model[0]}.length(), math::vector3{model[1]}.length(), math::vector3{model[2]}.length()});

          if ((math::vector3{world_center.x(), world_center.y(), world_center.z()} - light_position).length() > local_radius * scale + bounds.w()) {
            continue;
          }

          if (command.vertex_address_override != 0u) {
            is_dynamic = true;
          }

          utility::hash_combine(content, mesh_generation, material_generation, command.submesh_index, command.resident);
          hash_matrix(content, model);
        }
      }
    }

    auto key = content;
    hash_matrix(key, views[tile].view_projection);
    utility::hash_combine(key, views[tile].atlas_rect.x(), views[tile].atlas_rect.y(), views[tile].atlas_rect.z());

    // Clean only if last frame left exactly this content in exactly this rect.
    packet.local_shadow_dirty[tile] = (is_dynamic || !_local_shadow_keys.contains(key)) ? 1u : 0u;
    current_keys.insert(key);
  }

  _local_shadow_keys = std::move(current_keys);

  _local_shadow_stats = local_shadow_stats{
    .tile_count = static_cast<std::uint32_t>(views.size()),
    .rendered_count = static_cast<std::uint32_t>(std::ranges::count(packet.local_shadow_dirty, std::uint8_t{1u}))
  };
}

auto scene_renderer_module::request_cubemap_capture(const math::vector3& position, std::uint32_t resolution, std::float_t near_plane, std::float_t far_plane, cubemap_capture_callback on_complete) -> void {
  _capture_jobs.push_back(capture_job{position, std::clamp(resolution, 16u, 2048u), near_plane, far_plane, std::move(on_complete)});
}

auto scene_renderer_module::_advance_capture() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  _capture_face.reset();

  if (_capture_jobs.empty()) {
    return;
  }

  // Every face is recorded and submitted (the render thread is idle); wait for the GPU, then hand them over.
  if (_capture_next_face == cubemap_face_count) {
    graphics_module.logical_device().wait_idle();

    auto job = std::move(_capture_jobs.front());
    _capture_jobs.pop_front();
    _capture_next_face = 0u;

    auto capture = cubemap_capture{};
    capture.resolution = job.resolution;
    capture.pixels.resize(static_cast<std::size_t>(job.resolution) * job.resolution * cubemap_face_count);

    const auto* halves = static_cast<const std::uint16_t*>(registry.get<graphics::buffer>(_capture_readback_buffer).mapped());

    for (auto index = std::size_t{0u}; index < capture.pixels.size(); ++index) {
      capture.pixels[index] = math::color{half_to_float(halves[index * 4u + 0u]), half_to_float(halves[index * 4u + 1u]), half_to_float(halves[index * 4u + 2u]), half_to_float(halves[index * 4u + 3u])};
    }

    if (job.on_complete) {
      job.on_complete(std::move(capture));
    }

    if (_capture_jobs.empty()) {
      return;
    }
  }

  const auto resolution = _capture_jobs.front().resolution;

  if (_capture_readback_resolution != resolution) {
    if (_capture_readback_buffer.is_valid()) {
      registry.retire(_capture_readback_buffer, graphics_module.frame_context().frame_index());
    }

    _capture_readback_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = static_cast<std::size_t>(resolution) * resolution * cubemap_face_count * 4u * sizeof(std::uint16_t),
      .usage = graphics::buffer_usage::transfer_destination,
      .memory = graphics::memory_usage::host_read,
      .name = "Cubemap Capture Readback"
    });

    _capture_readback_resolution = resolution;
  }

  _capture_face = _capture_next_face++;
}

auto scene_renderer_module::_record_capture_copy(graphics::command_buffer& command_buffer) -> void {
  auto& registry = core::engine::get_module<graphics::graphics_module>().resource_registry();

  auto& color = registry.get<graphics::image>(_color_image);
  auto& readback = registry.get<graphics::buffer>(_capture_readback_buffer);

  const auto resolution = _capture_readback_resolution;

  // The graph leaves color read-only for tonemap_pass; borrow it for the copy and hand it back the same way.
  auto to_source = graphics::command_buffer::image_transition_data{};
  to_source.image = color;
  to_source.src_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  to_source.src_access_mask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  to_source.dst_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  to_source.dst_access_mask = VK_ACCESS_2_TRANSFER_READ_BIT;
  to_source.old_layout = graphics::image_layout::shader_read_only_optimal;
  to_source.new_layout = graphics::image_layout::transfer_source_optimal;
  command_buffer.transition_image_layout(to_source);

  auto region = VkBufferImageCopy{};
  region.bufferOffset = static_cast<VkDeviceSize>(*_capture_face) * resolution * resolution * 4u * sizeof(std::uint16_t);
  region.imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
  region.imageExtent = VkExtent3D{resolution, resolution, 1u};

  vkCmdCopyImageToBuffer(command_buffer.handle(), color.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.handle(), 1u, &region);

  auto to_read = graphics::command_buffer::image_transition_data{};
  to_read.image = color;
  to_read.src_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  to_read.src_access_mask = VK_ACCESS_2_TRANSFER_READ_BIT;
  to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  to_read.dst_access_mask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  to_read.old_layout = graphics::image_layout::transfer_source_optimal;
  to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
  command_buffer.transition_image_layout(to_read);

  auto to_host = VkMemoryBarrier2{};
  to_host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  to_host.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  to_host.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  to_host.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
  to_host.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
  command_buffer.memory_dependency(to_host);
}

auto scene_renderer_module::_gather_decals(render_packet& packet) -> void {
  auto& scene = core::engine::get_module<scenes::scenes_module>().active_scene();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto texture_index = [&](const assets::texture2d_handle& texture) {
    return texture.is_valid() && assets_module.is_resident(texture) ? texture->index() : 0xFFFFFFFFu;
  };

  struct sorted_decal {
    std::int32_t sort_order;
    math::uuid id;
    decal_data data;
  }; // struct sorted_decal

  auto decals = std::vector<sorted_decal>{};

  for (auto&& [entity, transform, decal, id] : scene.query<scenes::world_transform, scenes::decal, scenes::id>(ecs::exclude<scenes::inactive>).each()) {
    // Waits for a set texture rather than flashing the bare tint while it streams in.
    if ((decal.albedo.is_valid() && !assets_module.is_resident(decal.albedo)) || (decal.normal.is_valid() && !assets_module.is_resident(decal.normal))) {
      continue;
    }

    const auto& matrix = transform.matrix;

    auto data = decal_data{};
    data.world_to_decal = math::matrix4x4::inverted(matrix);
    data.center = math::vector4{math::vector3{matrix[3]}, 1.0f};
    data.axis_x = math::vector4{math::vector3{matrix[0]} * 0.5f, std::max(decal.normal_strength, 0.0f)};
    data.axis_y = math::vector4{math::vector3{matrix[1]} * 0.5f, std::cos(std::clamp(decal.angle_fade_start, 0.0f, decal.angle_fade_end))};
    data.axis_z = math::vector4{math::vector3{matrix[2]} * 0.5f, std::cos(std::clamp(decal.angle_fade_end, 0.0f, math::pif))};
    data.color = math::vector4{decal.color.r(), decal.color.g(), decal.color.b(), decal.color.a() * std::clamp(decal.opacity, 0.0f, 1.0f)};
    data.albedo_index = texture_index(decal.albedo);
    data.normal_index = texture_index(decal.normal);
    data.edge_fade = std::clamp(decal.edge_fade, 0.0f, 1.0f);

    decals.push_back(sorted_decal{decal.sort_order, static_cast<math::uuid>(id), data});
  }

  // Ties break by id, so equal sort orders don't swap from one frame to the next.
  std::ranges::sort(decals, [](const sorted_decal& lhs, const sorted_decal& rhs) {
    return lhs.sort_order != rhs.sort_order ? lhs.sort_order < rhs.sort_order : lhs.id < rhs.id;
  });

  packet.decals.reserve(decals.size());

  for (const auto& entry : decals) {
    packet.decals.push_back(entry.data);
  }
}

auto scene_renderer_module::_gather_reflection_probes(render_packet& packet) -> void {
  auto& scene = core::engine::get_module<scenes::scenes_module>().active_scene();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  struct sorted_probe {
    std::int32_t importance;
    std::float_t volume;
    math::uuid id;
    probe_data data;
  }; // struct sorted_probe

  auto probes = std::vector<sorted_probe>{};

  for (auto&& [entity, transform, probe, id] : scene.query<scenes::world_transform, scenes::reflection_probe, scenes::id>(ecs::exclude<scenes::inactive>).each()) {
    if (!probe.baked.is_valid() || !assets_module.is_resident(probe.baked)) {
      continue;
    }

    const auto& environment = *probe.baked;

    if (environment.irradiance_index() == assets::environment_map::invalid_index || environment.prefiltered_index() == assets::environment_map::invalid_index) {
      continue;
    }

    const auto box = reflection_probe_box(transform.matrix);
    const auto extents = math::vector3{std::max(probe.box_extents.x(), 0.001f), std::max(probe.box_extents.y(), 0.001f), std::max(probe.box_extents.z(), 0.001f)};

    auto data = probe_data{};
    data.world_to_box = math::matrix4x4::inverted(box);
    data.center = math::vector4{math::vector3{box[3]}, std::max(probe.blend_distance, 0.0f)};
    data.axis_x = math::vector4{math::vector3{box[0]} * extents.x(), std::max(probe.intensity, 0.0f)};
    data.axis_y = math::vector4{math::vector3{box[1]} * extents.y(), probe.box_projection ? 1.0f : 0.0f};
    data.axis_z = math::vector4{math::vector3{box[2]} * extents.z(), 0.0f};
    data.extents = math::vector4{extents, 0.0f};
    data.irradiance_index = environment.irradiance_index();
    data.prefiltered_index = environment.prefiltered_index();
    data.prefiltered_mip_count = environment.prefiltered_mip_count();

    probes.push_back(sorted_probe{probe.importance, extents.x() * extents.y() * extents.z(), static_cast<math::uuid>(id), data});
  }

  // A smaller box at equal importance is the more specific one, like a room inside a courtyard's probe.
  std::ranges::sort(probes, [](const sorted_probe& lhs, const sorted_probe& rhs) {
    if (lhs.importance != rhs.importance) {
      return lhs.importance > rhs.importance;
    }

    if (lhs.volume != rhs.volume) {
      return lhs.volume < rhs.volume;
    }

    return lhs.id < rhs.id;
  });

  packet.probes.reserve(probes.size());

  for (const auto& entry : probes) {
    packet.probes.push_back(entry.data);
  }
}

auto scene_renderer_module::_ensure_frame_capacity(const render_packet& packet) -> void {
  // Every cull view, plus the late camera view after them.
  const auto view_count = static_cast<std::size_t>(cull_view_count(packet));
  const auto transform_count = packet.transforms.size();
  const auto args_stride = static_cast<std::size_t>(packet.cull_args_stride);
  const auto transform_stride = static_cast<std::size_t>(packet.cull_transform_stride);

  auto replaced_tracked = false;

  // Grows by half again beyond what's needed, so a scene that keeps growing doesn't reallocate every frame.
  const auto grow = [](std::size_t& capacity, std::size_t needed) {
    if (needed <= capacity) {
      return false;
    }

    capacity = std::max(needed, capacity + capacity / 2u);
    return true;
  };

  if (grow(_transform_slot_capacity, transform_count)) {
    create_slotted_buffer(_transform_buffer, _transform_addresses, sizeof(transform_data), _transform_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::host_write, "Instance Transforms");
  }

  if (grow(_culled_indirect_args_slot_capacity, (view_count + 1u) * args_stride)) {
    // The recorded slots point into the old buffer.
    for (auto& layout : _lod_stat_layouts) {
      layout.clear();
    }

    create_slotted_buffer(_culled_indirect_args_buffer, _culled_indirect_args_addresses, sizeof(VkDrawIndexedIndirectCommand), _culled_indirect_args_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage | graphics::buffer_usage::indirect, graphics::memory_usage::host_write, "Culled Draw Args");
    replaced_tracked = true;
  }

  if (grow(_culled_transform_slot_capacity, (view_count + 1u) * transform_stride)) {
    create_slotted_buffer(_culled_transform_buffer, _culled_transform_addresses, sizeof(transform_data), _culled_transform_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::device_local, "Culled Instance Transforms");
    replaced_tracked = true;
  }

  if (grow(_instanced_culled_slot_capacity, packet.instanced_culled_count)) {
    create_slotted_buffer(_instanced_culled_buffer, _instanced_culled_addresses, sizeof(transform_data), _instanced_culled_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::device_local, "Culled Instanced Transforms");
    replaced_tracked = true;
  }

  if (grow(_occlusion_visibility_capacity, packet.occlusion_instance_count)) {
    _create_occlusion_visibility_buffer();
    replaced_tracked = true;
  }

  if (grow(_probe_slot_capacity, packet.probes.size())) {
    create_slotted_buffer(_probe_buffer, _probe_addresses, sizeof(probe_data), _probe_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::host_write, "Reflection Probes");
  }

  if (grow(_decal_slot_capacity, packet.decals.size())) {
    create_slotted_buffer(_decal_buffer, _decal_addresses, sizeof(decal_data), _decal_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::host_write, "Decals");
  }

  if (grow(_local_shadow_view_slot_capacity, packet.local_shadow_views.size())) {
    create_slotted_buffer(_local_shadow_view_buffer, _local_shadow_view_addresses, sizeof(local_shadow_view), _local_shadow_view_slot_capacity, graphics::buffer_usage::device_address | graphics::buffer_usage::storage, graphics::memory_usage::host_write, "Local Shadow Views");
  }

  // The graph's barriers name the cull buffers by handle.
  if (replaced_tracked) {
    _graph.compile(_build_graph_resources());
  }
}

auto scene_renderer_module::_build_graph_resources() const -> graph_resources {
  return graph_resources{
    .extent = _target_extent,
    .depth = _depth_image,
    .scene_depth = _scene_depth_image,
    .ambient_occlusion_raw = _ambient_occlusion_raw_image,
    .ambient_occlusion = _ambient_occlusion_image,
    .color = _color_image,
    .color_msaa = _color_msaa_image.is_valid() ? _color_msaa_image : _color_image,
    .ldr = _ldr_image,
    .final_image = _final_image,
    .accumulator = _accum_image,
    .accumulator_msaa = _accumulator_msaa_image.is_valid() ? _accumulator_msaa_image : _accum_image,
    .revealage = _revealage_image,
    .revealage_msaa = _revealage_msaa_image.is_valid() ? _revealage_msaa_image : _revealage_image,
    .bloom_downsample = _bloom_downsample_image,
    .bloom_upsample = _bloom_upsample_image,
    .shadow_maps = _shadow_map_images,
    .local_shadow_atlas = _local_shadow_atlas_image,
    .hiz = _hiz_image,
    .selection_mask = _selection_mask_image,
    .selection_depth = _selection_depth_image,
    .jump_flood = _jump_flood_images,
    .frame_buffer = _frame_buffer,
    .cluster_aabb_buffer = _cluster_aabb_buffer,
    .cluster_range_buffer = _cluster_range_buffer,
    .cluster_light_index_buffer = _cluster_light_index_buffer,
    .cluster_counter_buffer = _cluster_counter_buffer,
    .culled_indirect_args_buffer = _culled_indirect_args_buffer,
    .culled_transform_buffer = _culled_transform_buffer,
    .instanced_culled_buffer = _instanced_culled_buffer,
    .skin_scratch_buffer = _skin_scratch_buffer,
    .occlusion_visibility_buffer = _occlusion_visibility_buffer
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

  // _ensure_frame_capacity sized every per-slot region for this packet.
  const auto instance_count = static_cast<std::uint32_t>(context.packet->transforms.size());
  const auto view_count = cull_view_count(*context.packet);
  const auto command_stride = context.packet->cull_args_stride;

  // The late camera view sits after every regular cull view.
  const auto occlusion_enabled = context.packet->camera.is_active && (core::engine::has_project() ? core::engine::project().occlusion_culling() : core::project::default_occlusion_culling);
  context.occlusion_enabled = occlusion_enabled;
  context.late_view_index = view_count;
  context.hiz = _hiz_image;
  context.hiz_index = _hiz_index;
  context.hiz_storage_indices = _hiz_storage_indices;

  if (instance_count > 0u) {
    auto& transform_buffer = registry.get<graphics::buffer>(_transform_buffer);

    SBX_PROFILE_SCOPE("scene_renderer_module::write_transforms");

    transform_buffer.write(context.packet->transforms.data(), instance_count * sizeof(transform_data), context.slot * _transform_slot_capacity * sizeof(transform_data));
  }

  const auto& local_shadow_views = context.packet->local_shadow_views;

  if (!local_shadow_views.empty()) {
    registry.get<graphics::buffer>(_local_shadow_view_buffer).write(local_shadow_views.data(), local_shadow_views.size() * sizeof(local_shadow_view), context.slot * _local_shadow_view_slot_capacity * sizeof(local_shadow_view));
  }

  const auto& decals = context.packet->decals;

  if (!decals.empty()) {
    registry.get<graphics::buffer>(_decal_buffer).write(decals.data(), decals.size() * sizeof(decal_data), context.slot * _decal_slot_capacity * sizeof(decal_data));
  }

  const auto& probes = context.packet->probes;

  if (!probes.empty()) {
    registry.get<graphics::buffer>(_probe_buffer).write(probes.data(), probes.size() * sizeof(probe_data), context.slot * _probe_slot_capacity * sizeof(probe_data));
  }

  auto& culled_indirect_args_buffer = registry.get<graphics::buffer>(_culled_indirect_args_buffer);

  const auto reset_culled_indirect_args = [&](const std::vector<draw_command>& commands, std::uint32_t view) {
    SBX_PROFILE_SCOPE("scene_renderer_module::reset_culled_indirect_args");

    if (commands.empty()) {
      return;
    }

    // One per command and detail level, at the slots assign_lod_layout gave them; frustum_cull fills instanceCount.
    auto indirect_commands = std::vector<VkDrawIndexedIndirectCommand>(command_stride, VkDrawIndexedIndirectCommand{});

    for (const auto& command : commands) {
      for (auto level = std::uint32_t{0u}; level < command.lod_count; ++level) {
        const auto [index_count, first_index] = lod_index_range(command, level);

        auto& indirect_command = indirect_commands[command.lod_args_offset + level];
        indirect_command.indexCount = index_count;
        indirect_command.firstIndex = first_index;
      }
    }

    const auto offset = (context.slot * _culled_indirect_args_slot_capacity + static_cast<std::size_t>(view) * command_stride) * sizeof(VkDrawIndexedIndirectCommand);

    culled_indirect_args_buffer.write(indirect_commands.data(), indirect_commands.size() * sizeof(VkDrawIndexedIndirectCommand), offset);
  };

  // Before this slot's camera-view args are overwritten: the frame that last used it has finished, so its instance counts are final.
  {
    const auto* args = static_cast<const std::byte*>(culled_indirect_args_buffer.mapped());

    auto triangles = std::array<std::uint64_t, max_lod_levels>{};

    for (const auto& entry : _lod_stat_layouts[context.slot]) {
      auto indirect_command = VkDrawIndexedIndirectCommand{};
      std::memcpy(&indirect_command, args + entry.args_index * sizeof(VkDrawIndexedIndirectCommand), sizeof(indirect_command));

      triangles[entry.level] += static_cast<std::uint64_t>(indirect_command.instanceCount) * entry.triangles_per_instance;
    }

    for (auto level = std::size_t{0u}; level < max_lod_levels; ++level) {
      _lod_triangles[level].store(triangles[level], std::memory_order_relaxed);
    }

    auto& layout = _lod_stat_layouts[context.slot];
    layout.clear();

    // The camera view, and with occlusion culling the late camera view's additions.
    for (const auto view : {0u, occlusion_enabled ? view_count : 0xFFFFFFFFu}) {
      if (view == 0xFFFFFFFFu) {
        continue;
      }

      for (const auto& command : context.packet->opaque_commands) {
        for (auto level = std::uint32_t{0u}; level < command.lod_count; ++level) {
          layout.push_back(lod_stat_entry{
            .args_index = context.slot * _culled_indirect_args_slot_capacity + static_cast<std::size_t>(view) * command_stride + command.lod_args_offset + level,
            .level = level,
            .triangles_per_instance = lod_index_range(command, level).first / 3u
          });
        }
      }
    }
  }

  reset_culled_indirect_args(context.packet->opaque_commands, 0u);

  if (occlusion_enabled) {
    reset_culled_indirect_args(context.packet->opaque_commands, view_count);
  }

  // Like the LOD stats: the frame that last used this slot has finished, so its count is final.
  {
    auto& occlusion_counter_buffer = registry.get<graphics::buffer>(_occlusion_counter_buffer);
    auto* counter = static_cast<std::byte*>(occlusion_counter_buffer.mapped()) + context.slot * memory::stride_v<std::uint32_t>;

    auto culled = std::uint32_t{0u};
    std::memcpy(&culled, counter, sizeof(culled));
    _occlusion_culled.store(culled, std::memory_order_relaxed);

    const auto reset = std::uint32_t{0u};
    std::memcpy(counter, &reset, sizeof(reset));
  }

  if (context.packet->has_shadow_caster) {
    for (auto cascade = std::uint32_t{0u}; cascade < shadow_cascade_count; ++cascade) {
      reset_culled_indirect_args(context.packet->shadow_caster_commands, 1u + cascade);
    }
  }

  // Local light tiles cull independently of the sun, so they don't depend on has_shadow_caster.
  for (auto tile = shadow_cascade_count; tile + 1u < view_count; ++tile) {
    reset_culled_indirect_args(context.packet->shadow_caster_commands, 1u + tile);
  }

  const auto joint_count = std::min(static_cast<std::uint32_t>(context.packet->joint_matrices.size()), joint_palette_capacity);

  if (joint_count > 0u) {
    auto& joint_palette_buffer = registry.get<graphics::buffer>(_joint_palette_buffer);

    joint_palette_buffer.write(context.packet->joint_matrices.data(), joint_count * sizeof(math::matrix4x4), context.slot * joint_palette_capacity * sizeof(math::matrix4x4));
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

  const auto counter_reset = std::uint32_t{0u};
  auto& cluster_counter_buffer = registry.get<graphics::buffer>(_cluster_counter_buffer);
  cluster_counter_buffer.write(&counter_reset, sizeof(counter_reset), context.slot * memory::stride_v<std::uint32_t>);

  auto shadow_enabled = std::uint32_t{0u};
  auto cascade_splits = math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
  auto cascade_depth_bias_per_texel = math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
  auto cascade_texel_world_size = math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
  auto cascade_spheres = std::array<math::vector4, shadow_cascade_count>{};
  auto light_view_projections = std::array<math::matrix4x4, shadow_cascade_count>{};

  if (context.packet->has_shadow_caster && directional_light_count > 0u) {
    const auto light_direction = math::vector3{context.packet->lights[0].direction};
    const auto cascades = compute_cascades(camera, aspect, light_direction, context.packet->shadow_distance);

    for (auto i = std::size_t{0u}; i < shadow_cascade_count; ++i) {
      light_view_projections[i] = cascades[i].view_projection;
      cascade_spheres[i] = cascades[i].bounding_sphere;
    }

    cascade_splits = math::vector4{cascades[0].split_distance, cascades[1].split_distance, cascades[2].split_distance, cascades[3].split_distance};
    cascade_depth_bias_per_texel = math::vector4{cascades[0].depth_bias_per_texel, cascades[1].depth_bias_per_texel, cascades[2].depth_bias_per_texel, cascades[3].depth_bias_per_texel};
    cascade_texel_world_size = math::vector4{cascades[0].texel_world_size, cascades[1].texel_world_size, cascades[2].texel_world_size, cascades[3].texel_world_size};
    shadow_enabled = 1u;
  }

  context.has_shadow_caster = shadow_enabled != 0u;
  _has_rendered_shadows = context.has_shadow_caster;
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
  data.cascade_depth_bias_per_texel = cascade_depth_bias_per_texel;
  data.cascade_texel_world_size = cascade_texel_world_size;
  data.cascade_spheres = cascade_spheres;
  data.light_view_projections = light_view_projections;
  data.shadow_map_indices = _shadow_map_indices;
  data.shadow_enabled = shadow_enabled;
  data.frustum_planes = math::extract_frustum_planes(projection * context.packet->camera.view);
  data.scene_depth_index = _scene_depth_index;
  data.near_plane = camera.near_plane;
  data.far_plane = camera.far_plane;
  data.render_target_size = math::vector2{static_cast<std::float_t>(context.extent.x()), static_cast<std::float_t>(context.extent.y())};
  data.shadow_sampler_index = _shadow_sampler_index;
  data.shadow_cascade_debug = _shadow_cascade_debug_enabled ? 1u : 0u;
  data.shadow_depth_bias = context.packet->shadow_depth_bias;
  data.shadow_normal_bias = context.packet->shadow_normal_bias;
  data.shadow_penumbra_scale = std::tan(math::to_radians(math::degree{context.packet->shadow_angular_diameter}).value() * 0.5f);
  data.contact_shadow_length = context.packet->contact_shadow_length;
  data.ambient_occlusion_index = context.packet->camera.post_process.ambient_occlusion.enabled ? _ambient_occlusion_index : 0xFFFFFFFFu;
  data.local_shadow_view_count = static_cast<std::uint32_t>(local_shadow_views.size());
  data.local_shadow_view_address = _local_shadow_view_addresses[context.slot];
  data.local_shadow_atlas_index = _local_shadow_atlas_index;

  // An error of e world units at distance d covers e * lod_projection_scale / d pixels.
  const auto lod_bias = core::engine::has_project() ? core::engine::project().lod_bias() : core::project::default_lod_bias;
  data.lod_projection_scale = static_cast<std::float_t>(context.extent.y()) / (2.0f * std::tan(math::to_radians(math::degree{camera.fov_degrees}).value() * 0.5f));
  data.lod_error_threshold = lod_error_pixels * std::max(lod_bias, 0.01f);
  data.shadow_lod_error_threshold = data.lod_error_threshold * shadow_lod_error_scale;

  data.occlusion_visibility_address = _occlusion_visibility_address;
  data.occlusion_culled_counter_address = _occlusion_counter_addresses[context.slot];
  data.hiz_index = _hiz_index;
  data.hiz_width = hiz_width;
  data.hiz_height = hiz_height;
  data.hiz_mip_count = hiz_mip_count;
  data.occlusion_enabled = occlusion_enabled ? 1u : 0u;

  data.decal_address = _decal_addresses[context.slot];
  data.decal_count = static_cast<std::uint32_t>(decals.size());

  data.probe_address = _probe_addresses[context.slot];
  data.probe_count = static_cast<std::uint32_t>(probes.size());

  auto& frame_buffer = registry.get<graphics::buffer>(_frame_buffer);
  frame_buffer.write(&data, sizeof(frame_data), context.slot * memory::stride_v<frame_data>);

  context.frame_address = _frame_addresses[context.slot];
  context.transform_address = _transform_addresses[context.slot];
  context.joint_palette_address = _joint_palette_addresses[context.slot];
  context.instance_count = instance_count;
  context.sampler_index = _sampler_index;
  context.clamp_sampler_index = _clamp_sampler_index;
  context.show_grid = _grid_enabled && !_capture_face;
  context.wireframe = _wireframe_enabled && !_capture_face;
  context.is_capture = _capture_face.has_value();
  context.inverse_view_projection = math::matrix4x4::inverted(projection * context.packet->camera.view);

  context.cluster_aabb_address = _cluster_aabb_addresses[context.slot];
  context.cluster_range_address = data.cluster_range_address;
  context.cluster_light_index_address = data.cluster_light_index_address;
  context.cluster_counter_address = _cluster_counter_addresses[context.slot];

  context.culled_indirect_args_buffer = _culled_indirect_args_buffer;
  context.culled_indirect_args_address = _culled_indirect_args_addresses[context.slot];
  context.culled_indirect_args_slot_offset = static_cast<std::uint32_t>(context.slot * _culled_indirect_args_slot_capacity);
  context.culled_transform_address = _culled_transform_addresses[context.slot];
  context.culled_indirect_args_view_stride = command_stride;
  context.culled_transform_view_stride = context.packet->cull_transform_stride;
  context.instanced_culled_address = _instanced_culled_addresses[context.slot];

  const auto particle_write_index = static_cast<std::uint32_t>(context.frame_index % 2u);

  context.particle_additive_particles_address = _particle_pool_additive->particles_address();
  context.particle_additive_alive_list_address = _particle_pool_additive->alive_list_address(particle_write_index);
  context.particle_additive_emitters_address = _particle_pool_additive->emitter_instances_address(context.slot);
  context.particle_additive_draw_args = _particle_pool_additive->draw_args();

  context.particle_alpha_particles_address = _particle_pool_alpha_blend->particles_address();
  context.particle_alpha_alive_list_address = _particle_pool_alpha_blend->alive_list_address(particle_write_index);
  context.particle_alpha_emitters_address = _particle_pool_alpha_blend->emitter_instances_address(context.slot);
  context.particle_alpha_draw_args = _particle_pool_alpha_blend->draw_args();
}

} // namespace sbx::render
