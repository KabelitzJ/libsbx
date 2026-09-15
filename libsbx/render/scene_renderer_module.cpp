// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/scene_renderer_module.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/utility/hash.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/algorithm.hpp>
#include <libsbx/math/angle.hpp>
#include <libsbx/math/frustum.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/graphics/profiler.hpp>

#include <libsbx/assets/particle_effect.hpp>
#include <libsbx/assets/animation_graph.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/components.hpp>

#include <libsbx/render/passes/bloom_pass.hpp>
#include <libsbx/render/passes/frustum_cull_pass.hpp>
#include <libsbx/render/passes/depth_pre_pass.hpp>
#include <libsbx/render/passes/light_culling_pass.hpp>
#include <libsbx/render/passes/opaque_pass.hpp>
#include <libsbx/render/passes/skybox_pass.hpp>
#include <libsbx/render/passes/grid_pass.hpp>
#include <libsbx/render/passes/debug_draw_pass.hpp>
#include <libsbx/render/passes/tonemap_pass.hpp>
#include <libsbx/render/passes/canvas_pass.hpp>
#include <libsbx/render/passes/canvas_world_pass.hpp>
#include <libsbx/render/passes/transparent_accumulate_pass.hpp>
#include <libsbx/render/passes/particle_pass.hpp>
#include <libsbx/render/passes/transparent_resolve_pass.hpp>
#include <libsbx/render/particles/particle_pool.hpp>
#include <libsbx/render/particles/particle_simulate_pass.hpp>
#include <libsbx/render/skinning/skin_pass.hpp>
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
  math::vector4 cascade_depth_bias_per_texel;
  std::array<math::matrix4x4, shadow_cascade_count> light_view_projections;
  std::array<std::uint32_t, shadow_cascade_count> shadow_map_indices;
  std::uint32_t shadow_enabled;
  // World-space camera frustum planes (left, right, bottom, top, near, far), extracted once here
  // from view/projection above -- see math::extract_frustum_planes -- and read by
  // frustum_cull_pass.slang for its per-instance AABB test.
  std::array<math::vector4, 6u> frustum_planes;
}; // struct frame_data

// frustum_cull_pass tests a skinned draw_command's rest-pose local_bounds, not its actual animated
// pose -- padding by this fraction of the rest-pose bounds' own extent (in local space, so it scales
// with the instance's own world-transform scale -- see math::volume::inflated's doc comment) trades
// a little culling effectiveness for characters against not popping a wide swing/stretched limb out
// of view early at the frustum edge. A rigid (non-skinned) mesh's bounds are already exact and are
// left unpadded.
inline constexpr auto skinned_bounds_padding_factor = 0.2f;

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

// bit 0: double-sided (cull_mode::none vs back). bit 1: unlit shading model. Every draw-emitting
// pass's _pipelines array is laid out in this same order (see opaque_pass/transparent_accumulate_
// pass's constructors) so draw_command::pipeline_id indexes it directly with no further lookup.
auto compute_pipeline_id(const assets::material& material) -> std::uint32_t {
  return (material.is_double_sided() ? 1u : 0u) | (material.shading() == assets::shading_model::unlit ? 2u : 0u);
}

// Linear scan over a joint channel's keyframes -- clip sizes are small (tens of keys), so this
// isn't worth a binary search. Returns the same index twice when time falls outside the track
// (clamped to the first/last key) or the track has only one key.
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

// vector3 has no lerp of its own (math/vector3.hpp's is commented out) -- same per-component
// math::mix pattern sample_vector3_track above already uses.
auto mix_vector3(const math::vector3& start, const math::vector3& end, std::float_t t) -> math::vector3 {
  return math::vector3{
    math::mix(start.x(), end.x(), t),
    math::mix(start.y(), end.y(), t),
    math::mix(start.z(), end.z(), t)
  };
}

// Seeds every joint at bind pose, then overwrites the (sparse) joints @p clip actually animates --
// same two-step shape scene_renderer_module::_evaluate_skeleton_pose used to do inline for its one
// clip, factored out so it can be called once per side of a crossfade.
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

// Advances a local clip time by delta_time*speed and wraps/clamps it against duration -- same
// clamp-vs-loop logic the single-clip animator used to apply to its one time value, shared here
// between the current state and (mid-transition) the transition target, each of which has its own
// clip/speed/loop.
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

// All of a transition's conditions are ANDed by the caller; this checks one. An unknown parameter
// name or a condition authored against a different alternative than the parameter's current one
// both fail closed (return false) rather than throwing -- a misauthored graph just never takes
// that transition.
[[nodiscard]] auto evaluate_animation_condition(const assets::animation_condition& condition, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters) -> bool {
  const auto entry = std::ranges::find(parameters, condition.parameter_name, [](const auto& pair) { return std::string_view{pair.first}; });

  if (entry == parameters.end()) {
    return false;
  }

  const auto& value = entry->second;

  if (const auto* trigger = std::get_if<assets::animation_trigger>(&value)) {
    return trigger->set; // comparator/expected are meaningless for a trigger -- its presence having fired is the whole condition
  }

  return std::visit([&condition](const auto& current) -> bool {
    using current_type = std::decay_t<decltype(current)>;

    if constexpr (std::is_same_v<current_type, assets::animation_trigger>) {
      return false; // unreachable -- handled above
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

// Reconciles animator.parameters against graph.parameters() by name: adds any the graph defines
// that the instance doesn't have yet (seeded from the graph's default), and drops any the instance
// has that the graph no longer defines. Existing entries' current values are left untouched, so
// this doesn't clobber a live/test value while the graph is being edited. Needed because
// set_graph() only snapshots the parameter list once, at assignment time -- editing a graph's
// parameter list afterwards (e.g. live in animation_graph_panel) would otherwise never reach
// entities that already called set_graph() with it.
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

  // Sized well above particles_module's CPU-path defaults -- the GPU path exists specifically
  // for emitters wanting far more live particles than a per-particle CPU loop can afford.
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

  _graph.add_pass<skin_pass>();
  _graph.add_pass<frustum_cull_pass>();
  _graph.add_pass<depth_pre_pass>();
  _graph.add_pass<light_culling_pass>();
  _graph.add_pass<shadow_pass>();
  _graph.add_pass<opaque_pass>();
  _graph.add_pass<skybox_pass>();
  _graph.add_pass<grid_pass>();
  _graph.add_pass<debug_draw_pass>();
  _graph.add_pass<transparent_accumulate_pass>();
  _graph.add_pass<particle_simulate_pass>(*_particle_pool_additive, *_particle_pool_alpha_blend);
  _graph.add_pass<particle_pass>();
  _graph.add_pass<transparent_resolve_pass>();
  _graph.add_pass<canvas_world_pass>();
  _graph.add_pass<bloom_pass>();
  _graph.add_pass<tonemap_pass>();
  _graph.add_pass<canvas_pass>();

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

auto scene_renderer_module::set_viewport_offset(math::vector2 offset) -> void {
  _viewport_offset = offset;
}

auto scene_renderer_module::set_grid_enabled(bool enabled) -> void {
  _grid_enabled = enabled;
}

auto scene_renderer_module::grid_enabled() const -> bool {
  return _grid_enabled;
}

auto scene_renderer_module::reset_particles() -> void {
  _particle_pool_additive->clear();
  _particle_pool_alpha_blend->clear();
}

auto scene_renderer_module::_extract_gpu_particle_emitter(render_packet& packet, const assets::particle_emitter& config, scenes::particle_emitter& runtime, const scenes::particle_effect& instance, const math::matrix4x4& world, std::float_t delta_time) -> void {
  if (instance.playback != scenes::particle_playback_state::playing) {
    // Not playing: leave any claimed slot alone so particle_pool::tick() drains it gracefully
    // rather than an abrupt cut. Known v1 quirk: particle_simulate_pass has no pause concept, so a
    // *paused* effect's GPU particles keep drifting and draining instead of truly freezing.
    return;
  }

  const auto pool_index = config.blend_mode == assets::emitter_blend_mode::alpha_blend ? particle_pool_alpha_blend : particle_pool_additive;
  auto& pool = pool_index == particle_pool_alpha_blend ? *_particle_pool_alpha_blend : *_particle_pool_additive;

  if (runtime.slot == scenes::particle_emitter::invalid_slot) {
    const auto claimed = pool.claim_slot();

    if (!claimed) {
      return; // Pool exhausted; claim_slot already logged this once.
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
  data.seed = runtime.slot * 2654435761u; // combined with push.time in emit.slang -- doesn't need to change frame to frame.
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

  // Freshly assigned graph (set_graph can't resolve clips itself -- it has no mesh_renderer to
  // resolve names against), or a clip that failed to resolve last time (mesh not loaded yet):
  // keep retrying every frame until it sticks.
  if (!animator.current_clip.is_valid()) {
    animator.current_clip = _resolve_state_clip(renderer, current_state);
  }

  if (!animator.transition_target_state_id.has_value()) {
    if (animator.playing && current_state != nullptr) {
      const auto duration = animator.current_clip.is_valid() ? animator.current_clip->duration() : 0.0f;
      animator.current_time = advance_local_time(animator.current_time, delta_time, current_state->speed, duration, current_state->loop);
    }

    // Transitions are checked in authoring order -- the first whose conditions all pass wins,
    // same priority convention as Unity's Animator Controller.
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

  // Triggers are single-frame pulses -- consumed once this frame's transition checks have run,
  // whether or not one of them actually fired because of it.
  for (auto& [name, value] : animator.parameters) {
    if (auto* trigger = std::get_if<assets::animation_trigger>(&value)) {
      trigger->set = false;
    }
  }
}

auto scene_renderer_module::_evaluate_skeleton_pose(const assets::skeleton& skeleton, const scenes::mesh_renderer& renderer, scenes::animator* animator, scenes::skeleton_pose& pose, std::float_t delta_time) -> void {
  const auto& joints = skeleton.joints();
  const auto joint_count = joints.size();

  pose.joint_world_matrices.resize(joint_count);
  pose.skinning_matrices.resize(joint_count);

  const auto* graph = (animator != nullptr && animator->graph.is_valid()) ? animator->graph.get() : nullptr;

  if (graph != nullptr) {
    _advance_animator_state(renderer, *graph, *animator, delta_time);
  }

  // Seed every joint at bind pose first -- a clip's channels are sparse, so joints it doesn't
  // animate (and the whole skeleton, when there's no playing state at all) fall back to this.
  // Reused scratch storage (see the members' doc comment) instead of a fresh heap allocation per call.
  auto& translations = _skeleton_scratch_translations;
  auto& rotations = _skeleton_scratch_rotations;
  auto& scales = _skeleton_scratch_scales;

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

  // Mid-transition: sample the incoming state into its own bind-pose-seeded buffers and crossfade
  // per joint (lerp translation/scale, slerp rotation) by how far through the transition we are.
  if (graph != nullptr && animator->transition_target_state_id.has_value() && animator->transition_target_clip.is_valid()) {
    const auto alpha = (animator->transition_duration > 0.0f) ? std::clamp(animator->transition_time / animator->transition_duration, 0.0f, 1.0f) : 1.0f;

    auto& target_translations = _skeleton_scratch_target_translations;
    auto& target_rotations = _skeleton_scratch_target_rotations;
    auto& target_scales = _skeleton_scratch_target_scales;

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

  auto& locals = _skeleton_scratch_locals;
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
    data.exposure = camera.exposure;
    data.bloom_enabled = camera.bloom_enabled;
    data.bloom_intensity = camera.bloom_intensity;
    data.bloom_threshold = camera.bloom_threshold;
    data.bloom_knee = camera.bloom_knee;
    data.is_active = true;
  }

  return data;
}

auto scene_renderer_module::effective_camera() -> camera_data {
  return _resolve_camera_data();
}

auto scene_renderer_module::_build_packet() -> render_packet {
  SBX_PROFILE_SCOPE("scene_renderer_module::build_packet");

  auto packet = render_packet{};

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  packet.camera = _resolve_camera_data();

  if (scene.has_active_camera()) {
    auto camera_node = scene.active_camera();

    if (camera_node.has_component<scenes::skybox>()) {
      const auto& sky = camera_node.get_component<scenes::skybox>();

      packet.environment = sky.environment;
      packet.environment_intensity = sky.intensity;
      packet.ambient_intensity = sky.ambient_intensity;
    }

    if (_camera_override) {
      const auto& camera = camera_node.get_component<scenes::camera>();

      packet.camera.bloom_enabled = camera.bloom_enabled;
      packet.camera.bloom_intensity = camera.bloom_intensity;
      packet.camera.bloom_threshold = camera.bloom_threshold;
      packet.camera.bloom_knee = camera.bloom_knee;
    }
  }

  auto opaque = std::unordered_map<mesh_key, draw_bucket, mesh_key_hash>{};
  opaque.reserve(_last_opaque_bucket_count);

  auto transparent = std::vector<transparent_entry>{};

  for (const auto [entity, world, renderer] : scene.query<scenes::world_transform, scenes::mesh_renderer>(ecs::exclude<scenes::skeleton_pose, scenes::inactive>).each()) {
    if (!renderer.mesh.is_valid()) {
      continue;
    }

    if (renderer.mesh.is_loaded() && renderer.mesh->skeleton().is_valid()) {
      scene.node_of(entity).get_or_add_component<scenes::skeleton_pose>().skeleton = renderer.mesh->skeleton();
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

      const auto pipeline_id = compute_pipeline_id(*material);

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

  _last_opaque_bucket_count = opaque.size();

  auto ordered_opaque = std::vector<std::pair<mesh_key, draw_bucket>>{std::make_move_iterator(opaque.begin()), std::make_move_iterator(opaque.end())};
  std::ranges::sort(ordered_opaque, [](const mesh_key& lhs, const mesh_key& rhs) { return lhs < rhs; }, &std::pair<mesh_key, draw_bucket>::first);

  for (auto& [key, bucket] : ordered_opaque) {
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

  packet.transparent_commands.reserve(transparent.size());

  for (const auto& entry : transparent) {
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

  // Skinned meshes: each instance skins independently into its own scratch vertex range (no
  // cross-instance coalescing, unlike the static bucket above), and its pose is sampled/evaluated
  // here -- render cadence, feeding straight into this frame's packet -- rather than in a separate
  // fixed-tick system (see scenes::skeleton_pose's doc comment).
  auto skin_scratch_cursor = std::uint32_t{0u};
  const auto animation_delta_time = scenes_module.simulation_delta_time().value();

  for (const auto [entity, world, renderer, pose] : scene.query<scenes::world_transform, scenes::mesh_renderer, scenes::skeleton_pose>(ecs::exclude<scenes::inactive>).each()) {
    if (!renderer.mesh.is_valid() || !pose.skeleton.is_valid()) {
      continue;
    }

    auto node = scene.node_of(entity);
    auto animator_component = node.try_get_component<scenes::animator>();

    _evaluate_skeleton_pose(*pose.skeleton, renderer, animator_component.get(), pose, animation_delta_time);

    const auto instance_vertex_count = renderer.mesh->vertex_count();

    if (instance_vertex_count == 0u || !renderer.mesh->has_skin_data() || (skin_scratch_cursor + instance_vertex_count) > skin_scratch_vertex_capacity) {
      continue; // no skin data cooked, or this frame's scratch buffer is already full -- skip rather than overrun
    }

    const auto joint_offset = static_cast<std::uint32_t>(packet.joint_matrices.size());
    packet.joint_matrices.insert(packet.joint_matrices.end(), pose.skinning_matrices.begin(), pose.skinning_matrices.end());

    const auto output_vertex_address = _skin_scratch_address + static_cast<graphics::buffer::address_type>(skin_scratch_cursor) * sizeof(assets::vertex);

    packet.skin_dispatches.push_back(skin_dispatch{
      renderer.mesh->vertex_address(),
      renderer.mesh->skin_vertex_address(),
      output_vertex_address,
      joint_offset,
      instance_vertex_count
    });

    skin_scratch_cursor += instance_vertex_count;

    const auto& submeshes = renderer.mesh->submeshes();

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

  // Collected separately so the shadow-casting light can be swapped to lights[0] — shadow_pass and
  // the lighting shaders assume the caster is always index 0 when has_shadow_caster is set.
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
    }

    directional_lights.push_back(data);
  }

  if (shadow_caster_found && shadow_caster_index != 0u) {
    std::swap(directional_lights.front(), directional_lights[shadow_caster_index]);
  }

  packet.lights.insert(packet.lights.end(), directional_lights.begin(), directional_lights.end());

  packet.directional_light_count = static_cast<std::uint32_t>(packet.lights.size());

  for (auto&& [entity, transform, light] : scene.query<scenes::world_transform, scenes::point_light>(ecs::exclude<scenes::inactive>).each()) {
    const auto& matrix = transform.matrix;
    auto& out = packet.lights.emplace_back();

    out.type = light_type::point;
    out.color = math::vector4{light.color.r(), light.color.g(), light.color.b(), light.intensity};
    out.position = math::vector4{matrix[3].x(), matrix[3].y(), matrix[3].z(), light.range};
  }

  for (auto&& [entity, transform, light] : scene.query<scenes::world_transform, scenes::spot_light>(ecs::exclude<scenes::inactive>).each()) {
    const auto& matrix = transform.matrix;
    auto& out = packet.lights.emplace_back();

    out.type = light_type::spot;
    out.color = math::vector4{light.color.r(), light.color.g(), light.color.b(), light.intensity};
    out.position = math::vector4{math::vector3{matrix[3]}, light.range};
    out.direction = math::vector4{math::vector3::normalized(math::vector3{-matrix[2].x(), -matrix[2].y(), -matrix[2].z()}), 0.0f};
    out.inner_cos = std::cos(light.inner_angle);
    out.outer_cos = std::cos(light.outer_angle);
  }

  const auto delta_time = scenes_module.simulation_delta_time().value();
  const auto time = scenes_module.simulation_time().value();

  packet.delta_time = delta_time;
  packet.time = time;

  // No index buffer/adjacency concern for billboards (drawn from a plain instance buffer, not an
  // indexed mesh) -- unlike mesh_buckets below, iteration order genuinely doesn't matter here, so
  // this stays an unordered_map with no sort-back-to-order step needed.
  struct billboard_key_hash {
    auto operator()(const std::pair<std::uint32_t, assets::emitter_blend_mode>& key) const noexcept -> std::size_t {
      auto seed = std::hash<std::uint32_t>{}(key.first);
      utility::hash_combine(seed, key.second);
      return seed;
    }
  };

  auto billboard_buckets = std::unordered_map<std::pair<std::uint32_t, assets::emitter_blend_mode>, std::vector<particle_billboard_instance>, billboard_key_hash>{};

  // mesh_key doesn't carry a blend mode (ordinary meshes are either fully opaque or fully
  // transparent, never author-chosen additive/alpha_blend) -- particles need one, so pair it here
  // rather than extending the shared key type for a case only particles have.
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
  };

  struct particle_mesh_key_hash {
    auto operator()(const particle_mesh_key& value) const noexcept -> std::size_t {
      auto seed = mesh_key_hash{}(value.key);
      utility::hash_combine(seed, value.blend_mode);
      return seed;
    }
  };

  struct particle_mesh_bucket {
    assets::mesh_handle mesh;
    std::uint32_t submesh_index{0u};
    assets::material_handle material;
    assets::emitter_blend_mode blend_mode{};
    std::vector<particle_mesh_instance> instances;
  };

  // Unlike billboard/trail buckets above/below, mesh_buckets is sorted back into mesh_key order
  // once it's fully accumulated (same reasoning as the opaque bucket) -- particle_pass's mesh draw
  // loop rebinds the index buffer on every mesh change, so keeping same-mesh submeshes adjacent
  // still matters for draw-call efficiency even though correctness doesn't depend on it.
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

            // points.front() is the tail (oldest, ribbon_t = 1), points.back() the head (newest,
            // ribbon_t = 0) -- see particles::trail's doc comment.
            const auto ribbon_t = 1.0f - static_cast<std::float_t>(i) / static_cast<std::float_t>(point_count - 1u);

            auto color = config.trail.color_over_trail.has_keys() ? config.trail.color_over_trail.evaluate(ribbon_t) : point.color;

            const auto fade = config.trail.lifetime > 0.0f ? std::clamp(1.0f - point.age / config.trail.lifetime, 0.0f, 1.0f) : 1.0f;
            color.a() *= fade;

            // Local tangent along the ribbon, always pointing tail -> head (increasing i).
            const auto direction = (i + 1u < point_count)
              ? math::vector3::normalized(trail.points[i + 1u].position - point.position)
              : math::vector3::normalized(point.position - trail.points[i - 1u].position);

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

  // Once per frame, after every _extract_gpu_particle_emitter's keep_alive call above -- a slot
  // that was claimed but not kept alive this frame (emitter stopped/paused, or its node removed)
  // starts draining here instead of being recycled immediately. See particle_pool::tick's doc comment.
  _particle_pool_additive->tick(delta_time);
  _particle_pool_alpha_blend->tick(delta_time);

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
    .revealage_index = _revealage_index,
    .bloom_upsample = _bloom_upsample_image,
    .bloom_upsample_index = _bloom_upsample_index
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

  _bloom_upsample_index = bindless_table.reserve_sampled_image();

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

  // CPU writes indexCount/firstIndex/vertexOffset/firstInstance and resets instanceCount to 0 fresh
  // every frame (_prepare_frame, alongside the transform/frame buffer writes below); that host write
  // completes before the frame's command buffer is even submitted, so frustum_cull_pass's compute
  // shader only ever atomically increments instanceCount on top of already-settled data -- no race
  // with the CPU write, no separate device_local + copy step needed.
  _culled_indirect_args_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(VkDrawIndexedIndirectCommand) * max_opaque_draw_commands * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage | graphics::buffer_usage::indirect,
    .memory = graphics::memory_usage::host_write,
    .name = "Culled Draw Args"
  });

  const auto culled_indirect_args_base = registry.get<graphics::buffer>(_culled_indirect_args_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _culled_indirect_args_addresses[slot] = culled_indirect_args_base + slot * max_opaque_draw_commands * sizeof(VkDrawIndexedIndirectCommand);
  }

  // Same capacity/layout as _transform_buffer -- frustum_cull_pass compacts surviving instances into
  // this buffer at the exact same transform_offset each draw_command already has in the ordinary
  // transforms array (worst case every instance survives, needing that many contiguous slots either
  // way), so no separate offset bookkeeping is needed between the two buffers.
  _culled_transform_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(transform_data) * transform_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Culled Instance Transforms"
  });

  const auto culled_transform_base = registry.get<graphics::buffer>(_culled_transform_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _culled_transform_addresses[slot] = culled_transform_base + slot * transform_capacity * sizeof(transform_data);
  }

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
    .size = sizeof(assets::vertex) * skin_scratch_vertex_capacity,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Skin Scratch Vertices"
  });

  _skin_scratch_address = registry.get<graphics::buffer>(_skin_scratch_buffer).address();

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
    registry.retire(_bloom_downsample_image, frame_index);
    registry.retire(_bloom_upsample_image, frame_index);
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
    .bloom_downsample = _bloom_downsample_image,
    .bloom_upsample = _bloom_upsample_image,
    .shadow_maps = _shadow_map_images,
    .frame_buffer = _frame_buffer,
    .cluster_aabb_buffer = _cluster_aabb_buffer,
    .cluster_range_buffer = _cluster_range_buffer,
    .cluster_light_index_buffer = _cluster_light_index_buffer,
    .cluster_counter_buffer = _cluster_counter_buffer,
    .culled_indirect_args_buffer = _culled_indirect_args_buffer,
    .culled_transform_buffer = _culled_transform_buffer
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

  // frustum_cull_pass's input for this frame's opaque_commands: indexCount/firstIndex (this
  // command's submesh, unchanging whichever instances survive culling) known and written here;
  // instanceCount starts at 0 and is only ever atomically incremented by the compute pass, never
  // written by the CPU again after this -- see _culled_indirect_args_buffer's own doc comment on why
  // that ordering is race-free.
  const auto opaque_command_count = std::min(static_cast<std::uint32_t>(context.packet->opaque_commands.size()), max_opaque_draw_commands);

  if (opaque_command_count > 0u) {
    auto indirect_commands = std::vector<VkDrawIndexedIndirectCommand>{};
    indirect_commands.reserve(opaque_command_count);

    for (auto index = std::uint32_t{0u}; index < opaque_command_count; ++index) {
      const auto& command = context.packet->opaque_commands[index];

      auto indirect_command = VkDrawIndexedIndirectCommand{};

      if (command.mesh.is_valid() && command.submesh_index < command.mesh->submeshes().size()) {
        const auto& submesh = command.mesh->submeshes()[command.submesh_index];
        indirect_command.indexCount = submesh.index_count;
        indirect_command.firstIndex = submesh.index_offset;
      }

      indirect_command.instanceCount = 0u;
      indirect_command.vertexOffset = 0;
      indirect_command.firstInstance = 0u;

      indirect_commands.push_back(indirect_command);
    }

    auto& culled_indirect_args_buffer = registry.get<graphics::buffer>(_culled_indirect_args_buffer);

    culled_indirect_args_buffer.write(indirect_commands.data(), opaque_command_count * sizeof(VkDrawIndexedIndirectCommand), context.slot * max_opaque_draw_commands * sizeof(VkDrawIndexedIndirectCommand));
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

  // Reset before this frame's light_culling_pass dispatches its atomic-reserving cull_lights.slang.
  const auto counter_reset = std::uint32_t{0u};
  auto& cluster_counter_buffer = registry.get<graphics::buffer>(_cluster_counter_buffer);
  cluster_counter_buffer.write(&counter_reset, sizeof(counter_reset), context.slot * memory::stride_v<std::uint32_t>);

  // shadow_pass and lighting.slang assume lights[0] is the caster when has_shadow_caster is set (see reordering in _build_packet).
  auto shadow_enabled = std::uint32_t{0u};
  auto cascade_splits = math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
  auto cascade_depth_bias_per_texel = math::vector4{0.0f, 0.0f, 0.0f, 0.0f};
  auto light_view_projections = std::array<math::matrix4x4, shadow_cascade_count>{};

  if (context.packet->has_shadow_caster && directional_light_count > 0u) {
    const auto light_direction = math::vector3{context.packet->lights[0].direction};
    const auto cascades = compute_cascades(camera, aspect, light_direction, context.packet->shadow_distance);

    for (auto i = std::size_t{0u}; i < shadow_cascade_count; ++i) {
      light_view_projections[i] = cascades[i].view_projection;
    }

    cascade_splits = math::vector4{cascades[0].split_distance, cascades[1].split_distance, cascades[2].split_distance, cascades[3].split_distance};
    cascade_depth_bias_per_texel = math::vector4{cascades[0].depth_bias_per_texel, cascades[1].depth_bias_per_texel, cascades[2].depth_bias_per_texel, cascades[3].depth_bias_per_texel};
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
  data.cascade_depth_bias_per_texel = cascade_depth_bias_per_texel;
  data.light_view_projections = light_view_projections;
  data.shadow_map_indices = _shadow_map_indices;
  data.shadow_enabled = shadow_enabled;
  data.frustum_planes = math::extract_frustum_planes(projection * context.packet->camera.view);

  auto& frame_buffer = registry.get<graphics::buffer>(_frame_buffer);
  frame_buffer.write(&data, sizeof(frame_data), context.slot * memory::stride_v<frame_data>);

  context.frame_address = _frame_addresses[context.slot];
  context.transform_address = _transform_addresses[context.slot];
  context.joint_palette_address = _joint_palette_addresses[context.slot];
  context.instance_count = instance_count;
  context.sampler_index = _sampler_index;
  context.clamp_sampler_index = _clamp_sampler_index;
  context.show_grid = _grid_enabled;
  context.inverse_view_projection = math::matrix4x4::inverted(projection * context.packet->camera.view);

  context.cluster_aabb_address = _cluster_aabb_addresses[context.slot];
  context.cluster_range_address = data.cluster_range_address;
  context.cluster_light_index_address = data.cluster_light_index_address;
  context.cluster_counter_address = _cluster_counter_addresses[context.slot];

  context.culled_indirect_args_buffer = _culled_indirect_args_buffer;
  context.culled_indirect_args_address = _culled_indirect_args_addresses[context.slot];
  // Element (command-count) units, not bytes -- command_buffer::draw_indexed_indirect takes its
  // offset the same way draw_indexed_indirect_count does (see its own offset/count_buffer_offset),
  // multiplying by sizeof(VkDrawIndexedIndirectCommand) internally.
  context.culled_indirect_args_slot_offset = context.slot * max_opaque_draw_commands;
  context.culled_transform_address = _culled_transform_addresses[context.slot];

  // GPU-path particles: alive_list is ping-pong, keyed by frame_index % 2 (matching
  // particle_simulate_pass's write_index this frame). draw_args is always valid once the pool is
  // constructed; particle_pass checks its instance_count, not its existence, to know what to draw.
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
