// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/animation/pose_evaluation.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <libsbx/math/algorithm.hpp>
#include <libsbx/math/angle.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/assets/animation_clip.hpp>
#include <libsbx/assets/animation_graph.hpp>

namespace sbx::animation {

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

[[nodiscard]] auto find_animation_state(const assets::animation_layer& layer, std::uint32_t state_id) -> const assets::animation_state* {
  const auto& states = layer.states;
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

// A float parameter's value; ints count as floats and bools as 0 or 1, anything missing as 0.
auto float_parameter(const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters, std::string_view name) -> std::float_t {
  const auto entry = std::ranges::find(parameters, name, [](const auto& pair) { return std::string_view{pair.first}; });

  if (entry == parameters.end()) {
    return 0.0f;
  }

  if (const auto* value = std::get_if<std::float_t>(&entry->second)) {
    return *value;
  }

  if (const auto* value = std::get_if<std::int32_t>(&entry->second)) {
    return static_cast<std::float_t>(*value);
  }

  if (const auto* value = std::get_if<bool>(&entry->second)) {
    return *value ? 1.0f : 0.0f;
  }

  return 0.0f;
}

auto signed_angle(const math::vector2& from, const math::vector2& to) -> std::float_t {
  return std::atan2(from.x() * to.y() - from.y() * to.x(), from.x() * to.x() + from.y() * to.y());
}

auto length_of(const math::vector2& vector) -> std::float_t {
  return std::sqrt(vector.x() * vector.x() + vector.y() * vector.y());
}

// Gradient band interpolation (Johansen, "Automated Semi-Procedural Animation", as Unity's freeform blends): each child's weight is how far the point sits on its side of every band between it and another child.
// Directional measures the bands in polar space, by speed and angle, so a point between "walk forward" and "walk right" turns rather than cutting the corner through zero speed.
auto gradient_band_weights(std::span<const math::vector2> positions, const math::vector2& point, bool directional, std::span<std::float_t> weights) -> void {
  // How strongly angle differences count against speed differences in directional space.
  static constexpr auto angular_scale = 2.0f;

  const auto count = positions.size();

  for (auto i = std::size_t{0u}; i < count; ++i) {
    auto weight = 1.0f;

    for (auto j = std::size_t{0u}; j < count && weight > 0.0f; ++j) {
      if (i == j) {
        continue;
      }

      auto band = math::vector2{0.0f, 0.0f};
      auto offset = math::vector2{0.0f, 0.0f};

      if (directional) {
        const auto length_i = length_of(positions[i]);
        const auto length_j = length_of(positions[j]);
        const auto length_point = length_of(point);
        const auto average = std::max((length_i + length_j) * 0.5f, 1e-5f);

        // A child at the origin has no direction of its own; it takes its partner's, so the band is pure speed.
        const auto direction_i = length_i > 1e-5f ? positions[i] / length_i : (length_j > 1e-5f ? positions[j] / length_j : math::vector2{0.0f, 0.0f});
        const auto direction_j = length_j > 1e-5f ? positions[j] / length_j : direction_i;
        const auto direction_point = length_point > 1e-5f ? point / length_point : direction_i;

        band = math::vector2{(length_j - length_i) / average, signed_angle(direction_i, direction_j) * angular_scale};
        offset = math::vector2{(length_point - length_i) / average, signed_angle(direction_i, direction_point) * angular_scale};
      } else {
        band = positions[j] - positions[i];
        offset = point - positions[i];
      }

      const auto band_length_squared = band.x() * band.x() + band.y() * band.y();

      if (band_length_squared < 1e-10f) {
        continue;
      }

      weight = std::min(weight, std::clamp(1.0f - (offset.x() * band.x() + offset.y() * band.y()) / band_length_squared, 0.0f, 1.0f));
    }

    weights[i] = weight;
  }
}

// The weights of a blend's first weights.size() children, summing to 1 (all 0 when a direct blend's parameters are all 0).
auto blend_child_weights(const assets::animation_motion& motion, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters, std::span<std::float_t> weights) -> void {
  const auto count = weights.size();

  std::ranges::fill(weights, 0.0f);

  if (count == 0u) {
    return;
  }

  switch (motion.blend_type) {
    case assets::animation_blend_type::linear_1d: {
      const auto value = float_parameter(parameters, motion.parameter_x);

      // Thresholds needn't be authored in order.
      auto below = std::optional<std::size_t>{};
      auto above = std::optional<std::size_t>{};

      for (auto index = std::size_t{0u}; index < count; ++index) {
        const auto threshold = motion.children[index].threshold;

        if (threshold <= value && (!below || threshold > motion.children[*below].threshold)) {
          below = index;
        }

        if (threshold >= value && (!above || threshold < motion.children[*above].threshold)) {
          above = index;
        }
      }

      if (below && above && *below != *above) {
        const auto low = motion.children[*below].threshold;
        const auto high = motion.children[*above].threshold;
        const auto t = high > low ? (value - low) / (high - low) : 0.0f;

        weights[*below] = 1.0f - t;
        weights[*above] = t;
      } else {
        weights[below ? *below : *above] = 1.0f;
      }

      return;
    }
    case assets::animation_blend_type::freeform_directional_2d:
    case assets::animation_blend_type::freeform_cartesian_2d: {
      auto positions = std::array<math::vector2, assets::max_blend_children>{};

      for (auto index = std::size_t{0u}; index < count; ++index) {
        positions[index] = motion.children[index].position;
      }

      const auto point = math::vector2{float_parameter(parameters, motion.parameter_x), float_parameter(parameters, motion.parameter_y)};

      gradient_band_weights(std::span{positions.data(), count}, point, motion.blend_type == assets::animation_blend_type::freeform_directional_2d, weights);

      break;
    }
    case assets::animation_blend_type::direct: {
      for (auto index = std::size_t{0u}; index < count; ++index) {
        weights[index] = std::max(float_parameter(parameters, motion.children[index].direct_parameter), 0.0f);
      }

      break;
    }
  }

  auto total = 0.0f;

  for (const auto weight : weights) {
    total += weight;
  }

  if (total > 0.0f) {
    for (auto& weight : weights) {
      weight /= total;
    }
  }
}

// Appends the weight of every clip leaf under motion, depth first, scaled by weight; the order every per-leaf array of a state uses.
auto accumulate_leaf_weights(const assets::animation_motion& motion, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters, std::float_t weight, std::vector<std::float_t>& weights) -> void {
  if (motion.kind == assets::animation_motion_kind::clip) {
    weights.push_back(weight);
    return;
  }

  const auto count = std::min(motion.children.size(), assets::max_blend_children);

  auto child_weights = std::array<std::float_t, assets::max_blend_children>{};

  blend_child_weights(motion, parameters, std::span{child_weights.data(), count});

  for (auto index = std::size_t{0u}; index < count; ++index) {
    accumulate_leaf_weights(motion.children[index], parameters, weight * child_weights[index], weights);
  }
}

auto resolve_clip(const scenes::mesh_renderer& renderer, const std::string& name) -> assets::animation_clip_handle {
  if (!renderer.mesh.is_valid()) {
    return assets::animation_clip_handle{};
  }

  for (const auto& clip : renderer.mesh->animation_clips()) {
    if (clip.is_valid() && clip->name() == name) {
      return clip;
    }
  }

  return assets::animation_clip_handle{};
}

// Appends the clip of every leaf under motion, in accumulate_leaf_weights' order; an unresolved name leaves an invalid handle (that leaf holds the bind pose).
auto resolve_motion_clips(const scenes::mesh_renderer& renderer, const assets::animation_motion& motion, std::vector<assets::animation_clip_handle>& clips) -> void {
  if (motion.kind == assets::animation_motion_kind::clip) {
    clips.push_back(resolve_clip(renderer, motion.clip_name));
    return;
  }

  const auto count = std::min(motion.children.size(), assets::max_blend_children);

  for (auto index = std::size_t{0u}; index < count; ++index) {
    resolve_motion_clips(renderer, motion.children[index], clips);
  }
}

auto resolve_state_clips(const scenes::mesh_renderer& renderer, const assets::animation_state* state, std::vector<assets::animation_clip_handle>& clips) -> void {
  clips.clear();

  if (state != nullptr) {
    resolve_motion_clips(renderer, state->motion, clips);
  }
}

auto state_leaf_weights(const assets::animation_state* state, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters, std::vector<std::float_t>& weights) -> void {
  weights.clear();

  if (state != nullptr) {
    accumulate_leaf_weights(state->motion, parameters, 1.0f, weights);
  }
}

// One cycle of a blended state: its clips' durations averaged by weight, over the clips that resolved.
auto cycle_duration(const std::vector<assets::animation_clip_handle>& clips, const std::vector<std::float_t>& weights) -> std::float_t {
  auto duration = 0.0f;
  auto total = 0.0f;

  for (auto index = std::size_t{0u}; index < std::min(clips.size(), weights.size()); ++index) {
    if (clips[index].is_valid() && weights[index] > 0.0f) {
      duration += clips[index]->duration() * weights[index];
      total += weights[index];
    }
  }

  return total > 0.0f ? duration / total : 0.0f;
}

auto effective_speed(const assets::animation_state* state, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters) -> std::float_t {
  if (state == nullptr) {
    return 1.0f;
  }

  return state->speed_parameter.empty() ? state->speed : state->speed * float_parameter(parameters, state->speed_parameter);
}

// Whether one step of normalized time from `from` to `to` passed `point`; `to` is unwrapped for a looping state, so it can lie past 1 or below 0.
// inclusive also counts a point exactly at `from`, for the first step of a state that just started.
auto event_crossed(std::float_t from, std::float_t to, std::float_t point, bool loop, bool inclusive) -> bool {
  if (to == from) {
    return inclusive && point == from;
  }

  if (to > from) {
    if (!loop) {
      return (inclusive ? point >= from : point > from) && point <= to;
    }

    // The first repetition of the point at or after `from`.
    const auto first = point + (inclusive ? std::ceil(from - point) : std::floor(from - point) + 1.0f);

    return first <= to;
  }

  if (!loop) {
    return (inclusive ? point <= from : point < from) && point >= to;
  }

  // Playing backwards: the first repetition at or before `from`.
  const auto first = point + (inclusive ? std::floor(from - point) : std::ceil(from - point) - 1.0f);

  return first >= to;
}

// Appends the events of every leaf clip that playing from `from` to `to` passed, if the clip plays at enough weight.
auto emit_crossed_events(const std::vector<assets::animation_clip_handle>& clips, const std::vector<std::float_t>& weights, std::float_t from, std::float_t to, bool loop, bool inclusive, std::float_t state_weight, std::uint32_t layer_index, std::vector<fired_animation_event>& events) -> void {
  for (auto leaf = std::size_t{0u}; leaf < std::min(clips.size(), weights.size()); ++leaf) {
    if (!clips[leaf].is_valid()) {
      continue;
    }

    const auto& clip = *clips[leaf];
    const auto duration = clip.duration();
    const auto weight = weights[leaf] * state_weight;

    if (duration <= 0.0f || clip.events().empty()) {
      continue;
    }

    for (const auto& event : clip.events()) {
      if (weight < event.min_weight || !event_crossed(from, to, std::clamp(event.time / duration, 0.0f, 1.0f), loop, inclusive)) {
        continue;
      }

      events.push_back(fired_animation_event{
        .node = math::uuid::nil(),
        .name = event.name,
        .float_parameter = event.float_parameter,
        .int_parameter = event.int_parameter,
        .string_parameter = event.string_parameter,
        .clip_name = clip.name(),
        .layer = layer_index,
        .weight = weight
      });
    }
  }
}

// Wraps an angle into [-pi, pi].
auto wrap_angle(std::float_t angle) -> std::float_t {
  return std::remainder(angle, 2.0f * math::pif);
}

// A rotation's turn about the up (Y) axis: the twist of its swing-twist decomposition.
auto yaw_of(const math::quaternion& rotation) -> std::float_t {
  return 2.0f * std::atan2(rotation.y(), rotation.w());
}

// The root joint's mesh-space position and yaw in a clip at a time, the bind pose for whatever the clip doesn't animate.
auto sample_root(const assets::animation_clip& clip, const root_motion_context& context, std::float_t seconds) -> std::pair<math::vector3, std::float_t> {
  auto translation = context.bind_translation;
  auto rotation = context.bind_rotation;

  for (const auto& channel : clip.channels()) {
    if (channel.joint_index != static_cast<std::uint32_t>(context.joint)) {
      continue;
    }

    if (!channel.translation_keys.empty()) {
      translation = sample_vector3_track(channel.translation_keys, seconds);
    }

    if (!channel.rotation_keys.empty()) {
      rotation = sample_rotation_track(channel.rotation_keys, seconds);
    }

    break;
  }

  return {math::vector3{context.parent * math::vector4{translation, 1.0f}}, yaw_of(context.parent_rotation * rotation)};
}

// Adds the root motion of one step of a state's clips, each weighted, to the context; a looping clip that wrapped adds its whole cycles' displacement too.
auto accumulate_root_motion(const std::vector<assets::animation_clip_handle>& clips, const std::vector<std::float_t>& weights, std::float_t from, std::float_t to, bool loop, std::float_t state_weight, root_motion_context& context) -> void {
  for (auto leaf = std::size_t{0u}; leaf < std::min(clips.size(), weights.size()); ++leaf) {
    const auto weight = weights[leaf] * state_weight;

    if (!clips[leaf].is_valid() || weight <= 1e-4f) {
      continue;
    }

    const auto& clip = *clips[leaf];
    const auto duration = clip.duration();

    if (duration <= 0.0f) {
      continue;
    }

    const auto wraps = loop ? std::floor(to) : 0.0f;
    const auto [start_position, start_yaw] = sample_root(clip, context, from * duration);
    const auto [end_position, end_yaw] = sample_root(clip, context, (to - wraps) * duration);

    auto delta_position = end_position - start_position;
    auto delta_yaw = wrap_angle(end_yaw - start_yaw);

    if (wraps != 0.0f) {
      const auto [cycle_start_position, cycle_start_yaw] = sample_root(clip, context, 0.0f);
      const auto [cycle_end_position, cycle_end_yaw] = sample_root(clip, context, duration);

      delta_position = delta_position + (cycle_end_position - cycle_start_position) * wraps;
      delta_yaw += wrap_angle(cycle_end_yaw - cycle_start_yaw) * wraps;
    }

    context.delta_position = context.delta_position + delta_position * weight;
    context.delta_yaw += delta_yaw * weight;
  }
}

// Advances a state's normalized time by one step (wrapping when it loops, holding at the end otherwise) and emits the events its clips passed.
auto advance_state(const assets::animation_state* state, std::float_t& time, bool& fresh, const std::vector<assets::animation_clip_handle>& clips, const std::vector<std::float_t>& weights, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters, std::float_t delta_time, std::float_t state_weight, std::uint32_t layer_index, pose_scratch& scratch) -> void {
  if (state == nullptr) {
    return;
  }

  const auto duration = cycle_duration(clips, weights);
  const auto from = time;
  const auto unwrapped = duration > 0.0f ? time + delta_time * effective_speed(state, parameters) / duration : time;

  time = state->loop ? unwrapped - std::floor(unwrapped) : std::clamp(unwrapped, 0.0f, 1.0f);

  emit_crossed_events(clips, weights, from, state->loop ? unwrapped : time, state->loop, fresh, state_weight, layer_index, scratch.fired_events);

  // Root motion comes from the base layer only, like the pose it's taken out of.
  if (layer_index == 0u && scratch.root_motion.joint >= 0) {
    accumulate_root_motion(clips, weights, from, state->loop ? unwrapped : time, state->loop, state_weight, scratch.root_motion);
  }

  fresh = false;
}

auto reconcile_animator_layers(const assets::animation_graph& graph, scenes::animator& animator) -> void {
  const auto& layers = graph.layers();

  if (animator.layers.size() > layers.size()) {
    animator.layers.resize(layers.size());
  }

  while (animator.layers.size() < layers.size()) {
    const auto& layer = layers[animator.layers.size()];

    auto runtime = scenes::animator_layer{};
    runtime.current_state_id = layer.entry_state_id;
    runtime.weight = layer.weight;

    animator.layers.push_back(std::move(runtime));
  }
}

// One layer's state machine step: current state time, transition checks, and crossfade progress.
auto advance_layer(const scenes::mesh_renderer& renderer, const assets::animation_layer& layer, scenes::animator_layer& runtime, std::uint32_t layer_index, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters, bool playing, std::float_t delta_time, pose_scratch& scratch) -> void {
  // A state deleted from an edited graph restarts the layer at its entry state.
  if (find_animation_state(layer, runtime.current_state_id) == nullptr) {
    runtime.current_state_id = layer.entry_state_id;
    runtime.current_time = 0.0f;
    runtime.current_fresh = true;
    runtime.current_clips.clear();
    runtime.transition_target_state_id.reset();
  }

  // The base layer always plays at full weight; events weigh the layer's weight in like the pose does.
  const auto layer_weight = layer_index == 0u ? 1.0f : runtime.weight;

  // A paused animator, or one outside Play (no time passes), holds still and fires nothing.
  const auto advancing = playing && delta_time != 0.0f;

  if (runtime.transition_target_state_id.has_value() && find_animation_state(layer, *runtime.transition_target_state_id) == nullptr) {
    runtime.transition_target_state_id.reset();
    runtime.transition_target_clips.clear();
  }

  const auto* current_state = find_animation_state(layer, runtime.current_state_id);

  if (runtime.current_clips.empty()) {
    resolve_state_clips(renderer, current_state, runtime.current_clips);
  }

  state_leaf_weights(current_state, parameters, scratch.leaf_weights);

  if (!runtime.transition_target_state_id.has_value()) {
    if (advancing) {
      advance_state(current_state, runtime.current_time, runtime.current_fresh, runtime.current_clips, scratch.leaf_weights, parameters, delta_time, layer_weight, layer_index, scratch);
    }

    for (const auto& transition : layer.transitions) {
      if (transition.from_state.has_value() && *transition.from_state != runtime.current_state_id) {
        continue;
      }

      if (transition.has_exit_time && runtime.current_time < transition.exit_time) {
        continue;
      }

      const auto conditions_pass = std::ranges::all_of(transition.conditions, [&parameters](const auto& condition) {
        return evaluate_animation_condition(condition, parameters);
      });

      if (!conditions_pass) {
        continue;
      }

      runtime.transition_target_state_id = transition.to_state;
      runtime.transition_time = 0.0f;
      runtime.transition_target_time = 0.0f;
      runtime.target_fresh = true;
      runtime.transition_duration = transition.duration;
      resolve_state_clips(renderer, find_animation_state(layer, transition.to_state), runtime.transition_target_clips);

      break;
    }

    return;
  }

  const auto* target_state = find_animation_state(layer, *runtime.transition_target_state_id);

  if (runtime.transition_target_clips.empty()) {
    resolve_state_clips(renderer, target_state, runtime.transition_target_clips);
  }

  if (advancing) {
    state_leaf_weights(target_state, parameters, scratch.target_leaf_weights);

    runtime.transition_time += delta_time;

    // Events weigh in each side of the crossfade by the share it has after this step.
    const auto progress = (runtime.transition_duration > 0.0f) ? std::clamp(runtime.transition_time / runtime.transition_duration, 0.0f, 1.0f) : 1.0f;

    advance_state(current_state, runtime.current_time, runtime.current_fresh, runtime.current_clips, scratch.leaf_weights, parameters, delta_time, layer_weight * (1.0f - progress), layer_index, scratch);
    advance_state(target_state, runtime.transition_target_time, runtime.target_fresh, runtime.transition_target_clips, scratch.target_leaf_weights, parameters, delta_time, layer_weight * progress, layer_index, scratch);
  }

  const auto alpha = (runtime.transition_duration > 0.0f) ? std::clamp(runtime.transition_time / runtime.transition_duration, 0.0f, 1.0f) : 1.0f;

  if (alpha >= 1.0f) {
    runtime.current_state_id = *runtime.transition_target_state_id;
    runtime.current_time = runtime.transition_target_time;
    runtime.current_fresh = runtime.target_fresh;
    runtime.current_clips = std::move(runtime.transition_target_clips);

    runtime.transition_target_state_id.reset();
    runtime.transition_time = 0.0f;
    runtime.transition_target_time = 0.0f;
    runtime.transition_duration = 0.0f;
    runtime.transition_target_clips.clear();
  }
}

auto advance_animator_state(const scenes::mesh_renderer& renderer, const assets::animation_graph& graph, scenes::animator& animator, std::float_t delta_time, pose_scratch& scratch) -> void {
  reconcile_animator_parameters(graph, animator);
  reconcile_animator_layers(graph, animator);

  // An edited graph may have reshaped the trees the cached clips line up with, or the masks.
  if (animator.resolved_generation != graph.generation()) {
    animator.resolved_generation = graph.generation();

    for (auto& runtime : animator.layers) {
      runtime.current_clips.clear();
      runtime.transition_target_clips.clear();
      runtime.joint_weights.clear();
    }

    animator.root_motion_joint_index = -1;
  }

  for (auto index = std::size_t{0u}; index < animator.layers.size(); ++index) {
    advance_layer(renderer, graph.layers()[index], animator.layers[index], static_cast<std::uint32_t>(index), animator.parameters, animator.playing, delta_time, scratch);
  }

  // Every layer saw this frame's triggers; now they're spent.
  for (auto& [name, value] : animator.parameters) {
    if (auto* trigger = std::get_if<assets::animation_trigger>(&value)) {
      trigger->set = false;
    }
  }
}

// Samples a state's blended pose into translations/rotations/scales: every leaf with weight sampled at the shared normalized time and averaged, rotations in one hemisphere.
// Unresolved leaves contribute the bind pose; no weight at all leaves the bind pose.
auto sample_state_pose(const std::vector<assets::animation_clip_handle>& clips, const std::vector<std::float_t>& weights, std::float_t normalized_time, const std::vector<assets::skeleton::joint>& joints, std::vector<math::vector3>& translations, std::vector<math::quaternion>& rotations, std::vector<math::vector3>& scales, pose_scratch& scratch) -> void {
  const auto joint_count = joints.size();

  auto& leaf_translations = scratch.leaf_translations;
  auto& leaf_rotations = scratch.leaf_rotations;
  auto& leaf_scales = scratch.leaf_scales;

  leaf_translations.resize(joint_count);
  leaf_rotations.resize(joint_count);
  leaf_scales.resize(joint_count);

  for (auto index = std::size_t{0u}; index < joint_count; ++index) {
    translations[index] = math::vector3{0.0f, 0.0f, 0.0f};
    rotations[index] = math::quaternion{0.0f, 0.0f, 0.0f, 0.0f};
    scales[index] = math::vector3{0.0f, 0.0f, 0.0f};
  }

  auto total = 0.0f;

  for (auto leaf = std::size_t{0u}; leaf < std::min(clips.size(), weights.size()); ++leaf) {
    const auto weight = weights[leaf];

    if (weight <= 1e-4f) {
      continue;
    }

    for (auto index = std::size_t{0u}; index < joint_count; ++index) {
      const auto& joint = joints[index];
      leaf_translations[index] = joint.bind_local_translation;
      leaf_rotations[index] = joint.bind_local_rotation;
      leaf_scales[index] = joint.bind_local_scale;
    }

    if (clips[leaf].is_valid()) {
      const auto& clip = *clips[leaf];

      sample_clip_trs(clip, normalized_time * clip.duration(), joints, leaf_translations, leaf_rotations, leaf_scales);
    }

    for (auto index = std::size_t{0u}; index < joint_count; ++index) {
      translations[index] = translations[index] + leaf_translations[index] * weight;
      scales[index] = scales[index] + leaf_scales[index] * weight;

      // q and -q are the same rotation; add each in the hemisphere of what's accumulated so far.
      const auto sign = math::quaternion::dot(rotations[index], leaf_rotations[index]) < 0.0f ? -weight : weight;
      rotations[index] = rotations[index] + leaf_rotations[index] * sign;
    }

    total += weight;
  }

  if (total <= 0.0f) {
    for (auto index = std::size_t{0u}; index < joint_count; ++index) {
      const auto& joint = joints[index];
      translations[index] = joint.bind_local_translation;
      rotations[index] = joint.bind_local_rotation;
      scales[index] = joint.bind_local_scale;
    }

    return;
  }

  for (auto index = std::size_t{0u}; index < joint_count; ++index) {
    translations[index] = translations[index] / total;
    scales[index] = scales[index] / total;
    rotations[index] = math::quaternion::normalized(rotations[index]);
  }
}

// A layer's pose: its current state's motion, crossfaded into the transition target's while one is in flight.
// at_start samples both at normalized time 0, the reference an additive layer measures its motion against.
auto sample_layer_pose(const assets::animation_layer& layer, const scenes::animator_layer& runtime, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters, bool at_start, const std::vector<assets::skeleton::joint>& joints, std::vector<math::vector3>& translations, std::vector<math::quaternion>& rotations, std::vector<math::vector3>& scales, pose_scratch& scratch) -> void {
  const auto joint_count = joints.size();

  state_leaf_weights(find_animation_state(layer, runtime.current_state_id), parameters, scratch.leaf_weights);
  sample_state_pose(runtime.current_clips, scratch.leaf_weights, at_start ? 0.0f : runtime.current_time, joints, translations, rotations, scales, scratch);

  if (!runtime.transition_target_state_id.has_value()) {
    return;
  }

  const auto alpha = (runtime.transition_duration > 0.0f) ? std::clamp(runtime.transition_time / runtime.transition_duration, 0.0f, 1.0f) : 1.0f;

  auto& target_translations = scratch.target_translations;
  auto& target_rotations = scratch.target_rotations;
  auto& target_scales = scratch.target_scales;

  target_translations.resize(joint_count);
  target_rotations.resize(joint_count);
  target_scales.resize(joint_count);

  state_leaf_weights(find_animation_state(layer, *runtime.transition_target_state_id), parameters, scratch.target_leaf_weights);
  sample_state_pose(runtime.transition_target_clips, scratch.target_leaf_weights, at_start ? 0.0f : runtime.transition_target_time, joints, target_translations, target_rotations, target_scales, scratch);

  for (auto index = std::size_t{0u}; index < joint_count; ++index) {
    translations[index] = mix_vector3(translations[index], target_translations[index], alpha);
    rotations[index] = math::quaternion::slerp(rotations[index], target_rotations[index], alpha);
    scales[index] = mix_vector3(scales[index], target_scales[index], alpha);
  }
}

// A layer's mask as one weight per joint of this skeleton: 1 for joints it names (or every joint without a mask), 0 otherwise.
auto resolve_joint_weights(const assets::animation_layer& layer, const std::vector<assets::skeleton::joint>& joints, std::vector<std::float_t>& weights) -> void {
  weights.resize(joints.size());

  for (auto index = std::size_t{0u}; index < joints.size(); ++index) {
    weights[index] = (layer.mask.empty() || std::ranges::contains(layer.mask, joints[index].name)) ? 1.0f : 0.0f;
  }
}

// Blends a layer above the base over translations/rotations/scales, per joint by its weight times its mask.
auto apply_layer(const assets::animation_layer& layer, scenes::animator_layer& runtime, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters, const std::vector<assets::skeleton::joint>& joints, std::vector<math::vector3>& translations, std::vector<math::quaternion>& rotations, std::vector<math::vector3>& scales, pose_scratch& scratch) -> void {
  const auto joint_count = joints.size();

  if (runtime.weight <= 0.0f || find_animation_state(layer, runtime.current_state_id) == nullptr) {
    return;
  }

  if (runtime.joint_weights.size() != joint_count) {
    resolve_joint_weights(layer, joints, runtime.joint_weights);
  }

  auto& layer_translations = scratch.layer_translations;
  auto& layer_rotations = scratch.layer_rotations;
  auto& layer_scales = scratch.layer_scales;

  layer_translations.resize(joint_count);
  layer_rotations.resize(joint_count);
  layer_scales.resize(joint_count);

  sample_layer_pose(layer, runtime, parameters, false, joints, layer_translations, layer_rotations, layer_scales, scratch);

  if (layer.blend_mode == assets::animation_layer_blend_mode::override) {
    for (auto index = std::size_t{0u}; index < joint_count; ++index) {
      const auto weight = runtime.weight * runtime.joint_weights[index];

      if (weight <= 0.0f) {
        continue;
      }

      translations[index] = mix_vector3(translations[index], layer_translations[index], weight);
      rotations[index] = math::quaternion::slerp(rotations[index], layer_rotations[index], weight);
      scales[index] = mix_vector3(scales[index], layer_scales[index], weight);
    }

    return;
  }

  // Additive: the layer's change from its own first frame, added on top.
  auto& reference_translations = scratch.reference_translations;
  auto& reference_rotations = scratch.reference_rotations;
  auto& reference_scales = scratch.reference_scales;

  reference_translations.resize(joint_count);
  reference_rotations.resize(joint_count);
  reference_scales.resize(joint_count);

  sample_layer_pose(layer, runtime, parameters, true, joints, reference_translations, reference_rotations, reference_scales, scratch);

  const auto ratio = [](std::float_t value, std::float_t reference) {
    return std::abs(reference) > 1e-6f ? value / reference : 1.0f;
  };

  for (auto index = std::size_t{0u}; index < joint_count; ++index) {
    const auto weight = runtime.weight * runtime.joint_weights[index];

    if (weight <= 0.0f) {
      continue;
    }

    translations[index] = translations[index] + (layer_translations[index] - reference_translations[index]) * weight;

    const auto delta = math::quaternion::inverted(reference_rotations[index]) * layer_rotations[index];
    rotations[index] = math::quaternion::normalized(rotations[index] * math::quaternion::slerp(math::quaternion::identity, delta, weight));

    const auto scale_delta = math::vector3{ratio(layer_scales[index].x(), reference_scales[index].x()), ratio(layer_scales[index].y(), reference_scales[index].y()), ratio(layer_scales[index].z(), reference_scales[index].z())};
    scales[index] = scales[index] * mix_vector3(math::vector3{1.0f, 1.0f, 1.0f}, scale_delta, weight);
  }
}

// The root motion joint: the one the animator names, or else the first joint whose translation the base layer's clips animate.
// -1 while the clips are still loading (retried next frame), -2 when there's none.
auto resolve_root_joint(const scenes::animator& animator, const std::vector<assets::skeleton::joint>& joints) -> std::int32_t {
  if (!animator.root_motion_joint.empty()) {
    const auto entry = std::ranges::find(joints, animator.root_motion_joint, &assets::skeleton::joint::name);

    return entry != joints.end() ? static_cast<std::int32_t>(entry - joints.begin()) : -2;
  }

  if (animator.layers.empty() || animator.layers.front().current_clips.empty()) {
    return -1;
  }

  auto found = std::optional<std::uint32_t>{};

  for (const auto& clip : animator.layers.front().current_clips) {
    if (!clip.is_valid() || !clip.is_loaded()) {
      return -1;
    }

    for (const auto& channel : clip->channels()) {
      if (!channel.translation_keys.empty() && channel.joint_index < joints.size() && (!found || channel.joint_index < *found)) {
        found = channel.joint_index;
      }
    }
  }

  return found ? static_cast<std::int32_t>(*found) : -2;
}

// Sets up the instance's root motion for this frame, or leaves it off.
auto prepare_root_motion(const scenes::animator& animator, const std::vector<assets::skeleton::joint>& joints, root_motion_context& context) -> void {
  context = root_motion_context{};

  if (!animator.apply_root_motion || animator.root_motion_joint_index < 0 || static_cast<std::size_t>(animator.root_motion_joint_index) >= joints.size()) {
    return;
  }

  const auto joint = static_cast<std::size_t>(animator.root_motion_joint_index);

  // The parent chain in its bind pose: what the root joint moves relative to.
  auto chain = std::vector<std::size_t>{};

  for (auto parent = joints[joint].parent_index; parent >= 0; parent = joints[static_cast<std::size_t>(parent)].parent_index) {
    chain.push_back(static_cast<std::size_t>(parent));
  }

  for (const auto index : chain | std::views::reverse) {
    const auto& link = joints[index];

    context.parent = context.parent * compose_trs(link.bind_local_translation, link.bind_local_rotation, link.bind_local_scale);
    context.parent_rotation = context.parent_rotation * link.bind_local_rotation;
  }

  context.joint = static_cast<std::int32_t>(joint);
  context.bind_translation = joints[joint].bind_local_translation;
  context.bind_rotation = joints[joint].bind_local_rotation;
  context.extract_vertical = !animator.bake_vertical_root_motion;
  context.extract_rotation = !animator.bake_root_rotation;
}

// Takes the extracted motion out of the root joint's pose: its horizontal position (and height, and turn, when those are extracted) held at the bind pose's, so the mesh stays on its node.
auto remove_root_motion(const root_motion_context& context, std::vector<math::vector3>& translations, std::vector<math::quaternion>& rotations) -> void {
  const auto joint = static_cast<std::size_t>(context.joint);

  const auto bind_position = math::vector3{context.parent * math::vector4{context.bind_translation, 1.0f}};
  auto position = math::vector3{context.parent * math::vector4{translations[joint], 1.0f}};

  position.x() = bind_position.x();
  position.z() = bind_position.z();

  if (context.extract_vertical) {
    position.y() = bind_position.y();
  }

  translations[joint] = math::vector3{math::matrix4x4::inverted(context.parent) * math::vector4{position, 1.0f}};

  if (context.extract_rotation) {
    const auto mesh_rotation = context.parent_rotation * rotations[joint];
    const auto turn = yaw_of(context.parent_rotation * context.bind_rotation) - yaw_of(mesh_rotation);
    const auto unturned = math::quaternion{math::vector3::up, math::angle{math::radian{turn}}} * mesh_rotation;

    rotations[joint] = math::quaternion::normalized(math::quaternion::inverted(context.parent_rotation) * unturned);
  }
}

// The pose in mesh space: every joint's matrix and its rotation (the chain of local rotations, ignoring scale), for IK to measure and aim with.
auto compute_ik_globals(const std::vector<assets::skeleton::joint>& joints, const std::vector<math::vector3>& translations, const std::vector<math::quaternion>& rotations, const std::vector<math::vector3>& scales, pose_scratch& scratch) -> void {
  const auto joint_count = joints.size();

  scratch.ik_matrices.resize(joint_count);
  scratch.ik_rotations.resize(joint_count);

  for (auto index = std::size_t{0u}; index < joint_count; ++index) {
    const auto parent = joints[index].parent_index;
    const auto local = compose_trs(translations[index], rotations[index], scales[index]);

    scratch.ik_matrices[index] = parent < 0 ? local : scratch.ik_matrices[static_cast<std::size_t>(parent)] * local;
    scratch.ik_rotations[index] = parent < 0 ? rotations[index] : scratch.ik_rotations[static_cast<std::size_t>(parent)] * rotations[index];
  }
}

auto joint_position(const pose_scratch& scratch, std::size_t joint) -> math::vector3 {
  return math::vector3{scratch.ik_matrices[joint][3]};
}

// The shortest rotation taking one unit direction onto another.
auto rotation_between(const math::vector3& from, const math::vector3& to) -> math::quaternion {
  const auto cosine = math::vector3::dot(from, to);

  if (cosine < -0.9999f) {
    // Opposite directions: half a turn about any perpendicular axis.
    auto axis = math::vector3::cross(math::vector3{1.0f, 0.0f, 0.0f}, from);

    if (axis.length_squared() < 1e-6f) {
      axis = math::vector3::cross(math::vector3{0.0f, 1.0f, 0.0f}, from);
    }

    return math::quaternion{math::vector3::normalized(axis), math::angle{math::radian{math::pif}}};
  }

  const auto axis = math::vector3::cross(from, to);

  return math::quaternion::normalized(math::quaternion{axis.x(), axis.y(), axis.z(), 1.0f + cosine});
}

// Turns a joint by a mesh-space rotation, written into its local rotation.
auto rotate_joint(std::size_t joint, const math::quaternion& rotation, std::vector<math::quaternion>& rotations, const pose_scratch& scratch) -> void {
  const auto& global = scratch.ik_rotations[joint];

  rotations[joint] = math::quaternion::normalized(rotations[joint] * (math::quaternion::inverted(global) * rotation * global));
}

auto clamped_acos(std::float_t value) -> std::float_t {
  return std::acos(std::clamp(value, -1.0f, 1.0f));
}

// Analytic two-bone IK (after Holden): bends the root and middle joints so the end reaches the target, the limb bending toward the hint direction, all in mesh space.
auto solve_two_bone(const std::array<std::size_t, 3u>& chain, const math::vector3& target, const math::vector3& hint_direction, std::vector<math::quaternion>& rotations, const pose_scratch& scratch) -> void {
  static constexpr auto epsilon = 1e-4f;

  const auto a = joint_position(scratch, chain[0]);
  const auto b = joint_position(scratch, chain[1]);
  const auto c = joint_position(scratch, chain[2]);

  const auto upper = (b - a).length();
  const auto lower = (b - c).length();

  if (upper < epsilon || lower < epsilon) {
    return;
  }

  const auto reach = std::clamp((target - a).length(), epsilon, upper + lower - epsilon);

  const auto to_end = math::vector3::normalized(c - a);
  const auto to_middle = math::vector3::normalized(b - a);
  const auto to_target = math::vector3::normalized(target - a);

  const auto end_middle_0 = clamped_acos(math::vector3::dot(to_end, to_middle));
  const auto knee_0 = clamped_acos(math::vector3::dot(math::vector3::normalized(a - b), math::vector3::normalized(c - b)));
  const auto end_target_0 = clamped_acos(math::vector3::dot(to_end, to_target));

  // The angles the triangle of the two bones and the reach needs, by the law of cosines.
  const auto end_middle_1 = clamped_acos((lower * lower - upper * upper - reach * reach) / (-2.0f * upper * reach));
  const auto knee_1 = clamped_acos((reach * reach - upper * upper - lower * lower) / (-2.0f * upper * lower));

  auto bend_axis = math::vector3::cross(c - a, hint_direction);

  if (bend_axis.length_squared() < 1e-10f) {
    bend_axis = math::vector3::cross(c - a, b - a);
  }

  if (bend_axis.length_squared() < 1e-10f) {
    return;
  }

  bend_axis = math::vector3::normalized(bend_axis);

  const auto aim = math::vector3::cross(c - a, target - a);

  rotate_joint(chain[0], math::quaternion{bend_axis, math::angle{math::radian{end_middle_1 - end_middle_0}}}, rotations, scratch);
  rotate_joint(chain[1], math::quaternion{bend_axis, math::angle{math::radian{knee_1 - knee_0}}}, rotations, scratch);

  if (aim.length_squared() > 1e-10f) {
    rotate_joint(chain[0], math::quaternion{math::vector3::normalized(aim), math::angle{math::radian{end_target_0}}}, rotations, scratch);
  }
}

// A chain's joints: its end, the end's parent and grandparent.
auto resolve_ik_chain(scenes::animator_ik_chain& chain, const std::vector<assets::skeleton::joint>& joints) -> void {
  chain.joints = {-2, -2, -2};

  const auto end = std::ranges::find(joints, chain.end_joint, &assets::skeleton::joint::name);

  if (end == joints.end()) {
    return;
  }

  const auto end_index = static_cast<std::int32_t>(end - joints.begin());
  const auto middle_index = joints[static_cast<std::size_t>(end_index)].parent_index;

  if (middle_index < 0 || joints[static_cast<std::size_t>(middle_index)].parent_index < 0) {
    return;
  }

  chain.joints = {joints[static_cast<std::size_t>(middle_index)].parent_index, middle_index, end_index};
}

auto chain_is_resolved(const scenes::animator_ik_chain& chain, std::size_t joint_count) -> bool {
  return std::ranges::all_of(chain.joints, [joint_count](std::int32_t joint) { return joint >= 0 && static_cast<std::size_t>(joint) < joint_count; });
}

auto chain_joints(const scenes::animator_ik_chain& chain) -> std::array<std::size_t, 3u> {
  return {static_cast<std::size_t>(chain.joints[0]), static_cast<std::size_t>(chain.joints[1]), static_cast<std::size_t>(chain.joints[2])};
}

// One limb's goal for this frame in mesh space: the script's, or where foot placement wants it.
struct ik_goal {
  math::vector3 position{0.0f, 0.0f, 0.0f};
  std::float_t position_weight{0.0f};
  math::quaternion rotation{math::quaternion::identity}; // the end joint's mesh-space rotation
  std::float_t rotation_weight{0.0f};
  std::optional<math::vector3> hint{};
}; // struct ik_goal

// Foot placement: probes the ground under each foot chain, drops the pelvis so the lower foot can reach, and gives each foot a goal shifted by how far its ground sits from the node's, aligned to the ground's slope.
// Shifting rather than snapping keeps a foot lifted mid-step lifted.
auto place_feet(scenes::animator& animator, const std::vector<assets::skeleton::joint>& joints, std::vector<math::vector3>& translations, std::vector<math::quaternion>& rotations, const std::vector<math::vector3>& scales, std::float_t delta_time, pose_scratch& scratch, std::vector<ik_goal>& goals) -> void {
  if (!scratch.ground) {
    return;
  }

  const auto& world = scratch.world;
  const auto to_mesh = math::matrix4x4::inverted(world);
  const auto origin = math::vector3{world[3]};

  struct planted_foot {
    std::size_t chain;
    std::float_t offset;
    math::vector3 normal;
  }; // struct planted_foot

  auto planted = std::vector<planted_foot>{};
  auto pelvis = std::optional<std::size_t>{};
  auto lowest = 0.0f;

  for (auto index = std::size_t{0u}; index < animator.ik_chains.size(); ++index) {
    const auto& chain = animator.ik_chains[index];

    if (!chain.foot_placement || !chain_is_resolved(chain, joints.size()) || chain.position_weight > 0.0f) {
      continue;
    }

    const auto foot = math::vector3{world * math::vector4{joint_position(scratch, static_cast<std::size_t>(chain.joints[2])), 1.0f}};
    const auto hit = scratch.ground(math::vector3{foot.x(), origin.y() + animator.foot_reach, foot.z()}, animator.foot_reach * 2.0f, animator.foot_ground_layers);

    if (!hit) {
      continue;
    }

    const auto offset = hit->point.y() - origin.y();

    planted.push_back(planted_foot{index, offset, hit->normal});
    lowest = std::min(lowest, offset);

    if (!pelvis) {
      const auto hip_parent = joints[static_cast<std::size_t>(chain.joints[0])].parent_index;

      if (hip_parent >= 0) {
        pelvis = static_cast<std::size_t>(hip_parent);
      }
    }
  }

  // Eased, so stepping onto a ledge doesn't pop the hips.
  const auto ease = std::clamp(delta_time * 10.0f, 0.0f, 1.0f);
  animator.foot_pelvis_offset += (lowest - animator.foot_pelvis_offset) * ease;

  if (planted.empty()) {
    return;
  }

  if (pelvis) {
    const auto parent = joints[*pelvis].parent_index;
    const auto drop_mesh = math::vector3{to_mesh * math::vector4{0.0f, animator.foot_pelvis_offset, 0.0f, 0.0f}};
    const auto drop_local = parent < 0 ? drop_mesh : math::vector3{math::matrix4x4::inverted(scratch.ik_matrices[static_cast<std::size_t>(parent)]) * math::vector4{drop_mesh, 0.0f}};

    translations[*pelvis] = translations[*pelvis] + drop_local;

    compute_ik_globals(joints, translations, rotations, scales, scratch);
  }

  const auto mesh_up = math::vector3::normalized(math::vector3{to_mesh * math::vector4{0.0f, 1.0f, 0.0f, 0.0f}});

  for (const auto& foot : planted) {
    const auto end = static_cast<std::size_t>(animator.ik_chains[foot.chain].joints[2]);
    const auto current = math::vector3{world * math::vector4{joint_position(scratch, end), 1.0f}};
    const auto target = math::vector3{current.x(), current.y() + (foot.offset - animator.foot_pelvis_offset), current.z()};
    const auto normal_mesh = math::vector3::normalized(math::vector3{to_mesh * math::vector4{foot.normal, 0.0f}});

    auto& goal = goals[foot.chain];
    goal.position = math::vector3{to_mesh * math::vector4{target, 1.0f}};
    goal.position_weight = 1.0f;
    goal.rotation = rotation_between(mesh_up, normal_mesh) * scratch.ik_rotations[end];
    goal.rotation_weight = 1.0f;
  }
}

// Turns the head (and the spine joints above it, by falling shares) toward the look-at target, at most the clamp angle; the head's forward is the mesh's +Z in the bind pose, glTF's convention.
auto look_at(const scenes::animator& animator, const std::vector<assets::skeleton::joint>& joints, std::vector<math::vector3>& translations, std::vector<math::quaternion>& rotations, const std::vector<math::vector3>& scales, pose_scratch& scratch) -> void {
  const auto head = static_cast<std::size_t>(animator.look_at_joint_index);

  auto bind_rotation = joints[head].bind_local_rotation;

  for (auto parent = joints[head].parent_index; parent >= 0; parent = joints[static_cast<std::size_t>(parent)].parent_index) {
    bind_rotation = joints[static_cast<std::size_t>(parent)].bind_local_rotation * bind_rotation;
  }

  const auto forward_local = math::quaternion::inverted(bind_rotation) * math::vector3{0.0f, 0.0f, 1.0f};

  const auto target = math::vector3{math::matrix4x4::inverted(scratch.world) * math::vector4{animator.look_at_target, 1.0f}};
  const auto to_target = target - joint_position(scratch, head);

  if (to_target.length_squared() < 1e-8f) {
    return;
  }

  const auto forward = math::vector3::normalized(scratch.ik_rotations[head] * forward_local);
  const auto direction = math::vector3::normalized(to_target);
  const auto axis = math::vector3::cross(forward, direction);

  if (axis.length_squared() < 1e-10f) {
    return;
  }

  const auto angle = std::min(clamped_acos(math::vector3::dot(forward, direction)), animator.look_at_clamp) * std::clamp(animator.look_at_weight, 0.0f, 1.0f);
  const auto unit_axis = math::vector3::normalized(axis);

  // The head and the spine joints above it, head first; nearer the head turns more.
  auto chain = std::vector<std::size_t>{head};

  for (auto parent = joints[head].parent_index; parent >= 0 && chain.size() <= animator.look_at_spine_joints; parent = joints[static_cast<std::size_t>(parent)].parent_index) {
    chain.push_back(static_cast<std::size_t>(parent));
  }

  const auto count = static_cast<std::float_t>(chain.size());
  const auto total_share = count * (count + 1.0f) * 0.5f;

  // Spine first, so the head's share lands on top of the turned spine.
  for (auto index = chain.size(); index-- > 0u;) {
    const auto share = (count - static_cast<std::float_t>(index)) / total_share;

    rotate_joint(chain[index], math::quaternion{unit_axis, math::angle{math::radian{angle * share}}}, rotations, scratch);
    compute_ik_globals(joints, translations, rotations, scales, scratch);
  }
}

// IK on the finished pose: foot placement, every chain's goal, then look-at.
auto apply_ik(scenes::animator& animator, const std::vector<assets::skeleton::joint>& joints, std::vector<math::vector3>& translations, std::vector<math::quaternion>& rotations, const std::vector<math::vector3>& scales, std::float_t delta_time, pose_scratch& scratch) -> void {
  const auto joint_count = joints.size();

  for (auto& chain : animator.ik_chains) {
    if (chain.joints[0] == -1) {
      resolve_ik_chain(chain, joints);
    }
  }

  if (!animator.look_at_joint.empty() && animator.look_at_joint_index == -1) {
    const auto head = std::ranges::find(joints, animator.look_at_joint, &assets::skeleton::joint::name);
    animator.look_at_joint_index = head != joints.end() ? static_cast<std::int32_t>(head - joints.begin()) : -2;
  }

  const auto has_look_at = animator.look_at_joint_index >= 0 && static_cast<std::size_t>(animator.look_at_joint_index) < joint_count && animator.look_at_weight > 0.0f;

  const auto has_chain_work = std::ranges::any_of(animator.ik_chains, [&](const scenes::animator_ik_chain& chain) {
    return chain_is_resolved(chain, joint_count) && (chain.foot_placement || chain.position_weight > 0.0f || chain.rotation_weight > 0.0f);
  });

  if (!has_look_at && !has_chain_work) {
    return;
  }

  compute_ik_globals(joints, translations, rotations, scales, scratch);

  const auto to_mesh = math::matrix4x4::inverted(scratch.world);

  // The node's world rotation alone (its scale normalized out), to bring world rotations into mesh space.
  auto unscaled = scratch.world;

  for (auto column = std::size_t{0u}; column < 3u; ++column) {
    const auto axis = math::vector3{unscaled[column]};
    const auto length = axis.length();

    if (length > 1e-6f) {
      unscaled[column] = math::vector4{axis / length, 0.0f};
    }
  }

  unscaled[3] = math::vector4{0.0f, 0.0f, 0.0f, 1.0f};

  const auto node_rotation = math::quaternion{unscaled};

  // Script goals, in mesh space.
  auto goals = std::vector<ik_goal>(animator.ik_chains.size());

  for (auto index = std::size_t{0u}; index < animator.ik_chains.size(); ++index) {
    const auto& chain = animator.ik_chains[index];
    auto& goal = goals[index];

    goal.position = math::vector3{to_mesh * math::vector4{chain.target_position, 1.0f}};
    goal.position_weight = std::clamp(chain.position_weight, 0.0f, 1.0f);
    goal.rotation_weight = std::clamp(chain.rotation_weight, 0.0f, 1.0f);

    if (chain.has_hint) {
      goal.hint = math::vector3{to_mesh * math::vector4{chain.hint_position, 1.0f}};
    }

    goal.rotation = math::quaternion::normalized(math::quaternion::inverted(node_rotation) * chain.target_rotation);
  }

  place_feet(animator, joints, translations, rotations, scales, delta_time, scratch, goals);

  for (auto index = std::size_t{0u}; index < animator.ik_chains.size(); ++index) {
    const auto& chain = animator.ik_chains[index];
    const auto& goal = goals[index];

    if (!chain_is_resolved(chain, joint_count) || (goal.position_weight <= 0.0f && goal.rotation_weight <= 0.0f)) {
      continue;
    }

    const auto joints_of_chain = chain_joints(chain);
    const auto end = joints_of_chain[2];

    if (goal.position_weight > 0.0f) {
      const auto current = joint_position(scratch, end);
      const auto target = current + (goal.position - current) * goal.position_weight;
      const auto hint_direction = goal.hint ? *goal.hint - joint_position(scratch, joints_of_chain[0]) : joint_position(scratch, joints_of_chain[1]) - joint_position(scratch, joints_of_chain[0]);

      solve_two_bone(joints_of_chain, target, hint_direction, rotations, scratch);
      compute_ik_globals(joints, translations, rotations, scales, scratch);
    }

    if (goal.rotation_weight > 0.0f) {
      const auto& current = scratch.ik_rotations[end];
      const auto wanted = math::quaternion::slerp(current, goal.rotation, goal.rotation_weight);
      const auto parent = joints[end].parent_index;
      const auto parent_rotation = parent < 0 ? math::quaternion::identity : scratch.ik_rotations[static_cast<std::size_t>(parent)];

      rotations[end] = math::quaternion::normalized(math::quaternion::inverted(parent_rotation) * wanted);
      compute_ik_globals(joints, translations, rotations, scales, scratch);
    }
  }

  if (has_look_at) {
    look_at(animator, joints, translations, rotations, scales, scratch);
  }
}

auto evaluate_skeleton_pose(const assets::skeleton& skeleton, const scenes::mesh_renderer& renderer, memory::observer_ptr<scenes::animator> animator, scenes::skeleton_pose& pose, std::float_t delta_time, pose_scratch& scratch) -> void {
  const auto& joints = skeleton.joints();
  const auto joint_count = joints.size();

  pose.joint_world_matrices.resize(joint_count);
  pose.skinning_matrices.resize(joint_count);

  const auto* graph = (animator != nullptr && animator->graph.is_valid()) ? animator->graph.get() : nullptr;

  auto& translations = scratch.translations;
  auto& rotations = scratch.rotations;
  auto& scales = scratch.scales;

  translations.resize(joint_count);
  rotations.resize(joint_count);
  scales.resize(joint_count);

  scratch.root_motion = root_motion_context{};

  if (graph != nullptr) {
    // An edited graph resets the root joint with the clip caches (advance_animator_state), so it's re-resolved the frame after.
    if (animator->apply_root_motion && animator->root_motion_joint_index == -1) {
      animator->root_motion_joint_index = resolve_root_joint(*animator, joints);
    }

    prepare_root_motion(*animator, joints, scratch.root_motion);

    advance_animator_state(renderer, *graph, *animator, delta_time, scratch);
  }

  if (graph == nullptr || animator->layers.empty()) {
    for (auto index = std::size_t{0u}; index < joint_count; ++index) {
      const auto& joint = joints[index];
      translations[index] = joint.bind_local_translation;
      rotations[index] = joint.bind_local_rotation;
      scales[index] = joint.bind_local_scale;
    }
  } else {
    // The base layer always has full weight over every joint; the rest blend over it in order.
    sample_layer_pose(graph->layers().front(), animator->layers.front(), animator->parameters, false, joints, translations, rotations, scales, scratch);

    for (auto index = std::size_t{1u}; index < animator->layers.size(); ++index) {
      apply_layer(graph->layers()[index], animator->layers[index], animator->parameters, joints, translations, rotations, scales, scratch);
    }
  }

  if (animator != nullptr) {
    const auto& root = scratch.root_motion;

    if (root.joint >= 0) {
      remove_root_motion(root, translations, rotations);

      animator->root_motion_delta_position = math::vector3{root.delta_position.x(), root.extract_vertical ? root.delta_position.y() : 0.0f, root.delta_position.z()};
      animator->root_motion_delta_rotation = root.extract_rotation ? math::quaternion{math::vector3::up, math::angle{math::radian{root.delta_yaw}}} : math::quaternion::identity;
    } else {
      animator->root_motion_delta_position = math::vector3{0.0f, 0.0f, 0.0f};
      animator->root_motion_delta_rotation = math::quaternion::identity;
    }
  }

  if (animator != nullptr && graph != nullptr) {
    apply_ik(*animator, joints, translations, rotations, scales, delta_time, scratch);
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

} // namespace sbx::animation
