// SPDX-License-Identifier: MIT
#include <libsbx/animations/animator.hpp>

namespace sbx::animations {

static auto lerp_bone_transform(const animator::bone_transform& a, const animator::bone_transform& b, const std::float_t time) -> animator::bone_transform {
  return animator::bone_transform{
    math::vector3::lerp(a.position, b.position, time),
    math::quaternion::slerp(a.rotation, b.rotation,time),
    math::vector3::lerp(a.scale, b.scale, time)
  };
}

static auto sample_clip_locals(const skeleton& skeleton, const math::uuid& animation_id, const std::float_t time) -> std::vector<animator::bone_transform> {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& animation = assets_module.get_asset<animations::animation>(animation_id);

  auto locals = utility::make_vector<animator::bone_transform>(skeleton.bone_count());

  const auto& bones = skeleton.bones();
  const auto& tracks = animation.track_map();

  for (auto i = 0; i < skeleton.bone_count(); ++i) {
    const auto bone_name = skeleton.name_for_bone(i);

    auto entry = tracks.find(utility::hashed_string{bone_name});

    auto transform = animator::bone_transform{};

    if (entry != tracks.end()) {
      const auto& track = entry->second;

      transform.position = track.position_spline.sample(time);
      transform.rotation = math::quaternion::normalized(track.rotation_spline.sample(time));
      transform.scale = track.scale_spline.sample(time);
    } else {
      const auto& bind_matrix = bones[i].local_bind_matrix;

      const auto [position, rotation, scale] = math::decompose(bind_matrix);

      transform.position = position;
      transform.rotation = rotation;
      transform.scale = scale;
    }

    locals[i] = transform;
  }

  return locals;
}

// static auto locals_to_final_matrices(const skeleton& skeleton, const std::vector<animator::bone_transform>& local_transforms) -> std::vector<math::matrix4x4> {
//   const auto bone_count = skeleton.bone_count();

//   auto global_matrices = utility::make_vector<math::matrix4x4>(bone_count, math::matrix4x4::identity);
//   auto final_matrices = utility::make_vector<math::matrix4x4>(bone_count, math::matrix4x4::identity);

//   const auto& bones = skeleton.bones();

//   for (std::uint32_t bone_index = 0; bone_index < bone_count; ++bone_index) {
//     const auto& transform = local_transforms[bone_index];

//     const auto translation_matrix = math::matrix4x4::translated(math::matrix4x4::identity, transform.position);
//     const auto rotation_matrix = math::matrix_cast<math::matrix4x4>(transform.rotation);
//     const auto scale_matrix = math::matrix4x4::scaled(math::matrix4x4::identity, transform.scale);

//     const auto local_matrix = translation_matrix * rotation_matrix * scale_matrix;

//     const auto global_matrix = (bones[bone_index].parent_id != skeleton::bone::null) ? global_matrices[bones[bone_index].parent_id] * local_matrix : local_matrix;

//     final_matrices[bone_index] = skeleton.inverse_root_transform() * global_matrix * bones[bone_index].inverse_bind_matrix;
//     global_matrices[bone_index] = std::move(global_matrix);
//   }

//   return final_matrices;
// }

static auto _clip_duration(const math::uuid& animation_id) ->  std::float_t {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto& clip = assets_module.get_asset<animations::animation>(animation_id);

  return clip.duration();
}

static auto _normalized_time(const animator::state& state, const std::float_t time) -> std::float_t {
  const auto duration = _clip_duration(state.animation_id);

  if (duration <= 1e-6f) {
    return 0.0f;
  }

  return std::fmod(std::max(time, 0.0f), duration) / duration;
}

auto animator::add_state(const state& state) -> void {
  _state_map[state.name] = state;
}

auto animator::add_transition(transition&& transition) -> void {
  _transitions.push_back(std::move(transition));
}

auto animator::set_bool(const utility::hashed_string& key, bool value) -> void { 
  _parameters.bool_values[key] = value; 
}

auto animator::set_float(const utility::hashed_string& key, std::float_t value) -> void { 
  _parameters.float_values[key] = value; 
}

auto animator::set_trigger(const utility::hashed_string& key) -> void { 
  _parameters.trigger_values[key] = true; 
}

auto animator::reset_trigger(const utility::hashed_string& key) -> void { 
  _parameters.trigger_values[key] = false; 
}

auto animator::play(const utility::hashed_string& state_name, const bool instant, const std::float_t cross_fade) -> void {
  const auto entry = _state_map.find(state_name);

  if (entry == _state_map.end()) {
    return;
  }

  _start_playback(entry->second, instant, cross_fade);
}

void animator::update(const std::float_t delta_time) {
  if (!_has_valid_clip(_current_state)) {
    return;
  }

  _current_state_time += delta_time * _current_state.speed;
  _current_state_time = _wrap_state_time(_current_state, _current_state_time);

  if (_is_in_transition && _has_valid_clip(_next_state)) {
    _next_state_time += delta_time * _next_state.speed;
    _next_state_time = _wrap_state_time(_next_state, _next_state_time);

    _cross_fade_elapsed = std::min(_cross_fade_elapsed + delta_time, _cross_fade_duration);

    if (_cross_fade_elapsed >= _cross_fade_duration) {
      _current_state = _next_state;
      _current_state_time = _next_state_time;
      _is_in_transition = false;
      _next_state = {};
      _next_state_time = 0.0f;
    }
  } else {
    for (const auto& rule : _transitions) {
      if (rule.from != _current_state.name) {
        continue;
      }

      if (rule.condition && !rule.condition(*this)) {
        continue;
      }

      if (rule.exit_time) {
        const std::float_t normalized_time = _normalized_time(_current_state, _current_state_time);

        if (normalized_time + 1e-6f < *rule.exit_time) {
          continue;
        }
      }

      const auto entry = _state_map.find(rule.to);

      if (entry == _state_map.end()) {
        continue;
      }

      _next_state = entry->second;
      _next_state_time = 0.0f;
      _is_in_transition = (rule.duration > 0.0f);
      _cross_fade_duration = std::max(0.0f, rule.duration);
      _cross_fade_elapsed = 0.0f;

      if (!_is_in_transition) {
        _current_state = _next_state;
        _current_state_time = 0.0f;
        _next_state = {};
      }

      break;
    }
  }

  for (auto& [key, value] : _parameters.trigger_values) {
    value = false;
  }
}

auto animator::evaluate_locals(const skeleton& skeleton) -> std::vector<bone_transform> {
  if (!_has_valid_clip(_current_state)) {
    auto locals = utility::make_vector<bone_transform>(skeleton.bone_count());
    const auto& bones = skeleton.bones();

    for (auto i = 0; i < skeleton.bone_count(); ++i) {
      const auto [p, r, s] = math::decompose(bones[i].local_bind_matrix);
      locals[i] = bone_transform{p, r, s};
    }

    return locals;
  }

  if (_is_in_transition && _has_valid_clip(_next_state) && _cross_fade_duration > 0.0f) {
    const auto blend = std::clamp(_cross_fade_elapsed / _cross_fade_duration, 0.0f, 1.0f);
    auto a = sample_clip_locals(skeleton, _current_state.animation_id, _current_state_time);
    auto b = sample_clip_locals(skeleton, _next_state.animation_id, _next_state_time);

    auto out = utility::make_vector<bone_transform>(a.size());

    for (size_t i = 0; i < out.size(); ++i) {
      out[i] = lerp_bone_transform(a[i], b[i], blend);
    }

    return out;
  }

  return sample_clip_locals(skeleton, _current_state.animation_id, _current_state_time);
}

static auto locals_to_globals(const skeleton& skeleton, const std::vector<animator::bone_transform>& local_transforms) -> std::vector<math::matrix4x4> {
  const auto bone_count = skeleton.bone_count();
  const auto& bones = skeleton.bones();

  auto global = utility::make_vector<math::matrix4x4>(bone_count, math::matrix4x4::identity);

  for (auto i = 0; i < bone_count; ++i) {
    const auto& t = local_transforms[i];

    const auto T = math::matrix4x4::translated(math::matrix4x4::identity, t.position);
    const auto R = math::matrix_cast<math::matrix4x4>(t.rotation);
    const auto S = math::matrix4x4::scaled(math::matrix4x4::identity, t.scale);

    const auto local = T * R * S;

    global[i] = (bones[i].parent_id != skeleton::bone::null) ? global[bones[i].parent_id] * local : local;
  }

  return global;
}

static auto globals_to_skin(const skeleton& skeleton, const std::vector<math::matrix4x4>& globals) -> std::vector<math::matrix4x4> {
  const auto bone_count = skeleton.bone_count();
  const auto& bones = skeleton.bones();

  auto skin = utility::make_vector<math::matrix4x4>(bone_count, math::matrix4x4::identity);

  for (auto i = 0; i < bone_count; ++i) {
    skin[i] = skeleton.inverse_root_transform() * globals[i] * bones[i].inverse_bind_matrix;
  }

  return skin;
}

auto animator::evaluate_pose(const skeleton& skeleton, const std::vector<bone_transform>& locals) -> std::vector<math::matrix4x4> {
  utility::assert_that(skeleton.bone_count() == locals.size(), "Skeleton missmatch");

  if (!_has_valid_clip(_current_state)) {
    return utility::make_vector<math::matrix4x4>(skeleton.bone_count(), math::matrix4x4::identity);
  }

  auto globals = locals_to_globals(skeleton, locals);

  return globals_to_skin(skeleton, globals);
}

auto animator::current_state_name() const -> const utility::hashed_string& { 
  return _current_state.name; 
}

auto animator::is_in_transition() const -> bool { 
  return _is_in_transition;
}

auto animator::bool_parameter(const utility::hashed_string& key) const -> std::optional<bool> {
  if (const auto entry = _parameters.bool_values.find(key); entry != _parameters.bool_values.cend()) {
    return entry->second;
  }

  return std::nullopt;
}

auto animator::float_parameter(const utility::hashed_string& key) const -> std::optional<std::float_t> {
  if (const auto entry = _parameters.float_values.find(key); entry != _parameters.float_values.cend()) {
    return entry->second;
  }

  return std::nullopt;
}

auto animator::trigger_parameter(const utility::hashed_string& key) const -> std::optional<bool> {
  if (const auto entry = _parameters.trigger_values.find(key); entry != _parameters.trigger_values.cend()) {
    return entry->second;
  }

  return std::nullopt;
}

auto animator::_start_playback(const state& target_state, const bool instant, const std::float_t cross_fade) -> void {
  if (instant || !_has_valid_clip(_current_state) || cross_fade <= 0.0f) {
    _current_state = target_state;
    _current_state_time = 0.0f;
    _is_in_transition = false;
  } else {
    _next_state = target_state;
    _next_state_time = 0.0f;
    _is_in_transition = true;
    _cross_fade_duration = cross_fade;
    _cross_fade_elapsed = 0.0f;
  }
}

auto animator::_wrap_state_time(const state& state, std::float_t time) const -> std::float_t {
  const auto duration = _clip_duration(state.animation_id);

  if (duration <= 1e-6f) {
    return 0.0f;
  }

  if (!state.is_looping) {
    return std::clamp(time, 0.0f, duration);
  }

  auto wrapped = std::fmod(time, duration);

  return wrapped < 0.0f ? wrapped + duration : wrapped;
}

auto animator::_has_valid_clip(const state& state) const -> bool {
  return state.animation_id != math::uuid::null();
}

} // namespace sbx::animations 
