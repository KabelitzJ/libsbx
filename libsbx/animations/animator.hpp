// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ANIMATIONS_ANIMATOR_HPP_
#define LIBSBX_ANIMATIONS_ANIMATOR_HPP_

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/matrix_cast.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/animations/skeleton.hpp>

namespace sbx::animations {

class animator {

public:

  struct state {
    utility::hashed_string name;
    math::uuid animation_id{};
    bool is_looping = true;
    std::float_t speed = 1.0f;
  }; // struct state

  struct transition {
    utility::hashed_string from;
    utility::hashed_string to;
    std::float_t duration = 0.25f;
    std::function<bool(const animator&)> condition;
    std::optional<std::float_t> exit_time = 0.0f;
  }; // struct transition

  struct parameters {
    std::unordered_map<utility::hashed_string, bool> bool_values;
    std::unordered_map<utility::hashed_string, std::float_t> float_values;
    std::unordered_map<utility::hashed_string, bool> trigger_values;
  }; // struct parameters

  struct bone_transform {
    math::vector3 position{math::vector3::zero};
    math::quaternion rotation{math::quaternion::identity};
    math::vector3 scale{math::vector3::one};
  }; // struct bone_transform

  animator() {
    _current_state.animation_id = math::uuid::null();
  }

  auto add_state(const state& new_state) -> void;

  auto add_transition(transition&& transition) -> void;

  auto set_bool(const utility::hashed_string& key, bool value) -> void;

  auto set_float(const utility::hashed_string& key, std::float_t value) -> void;

  auto set_trigger(const utility::hashed_string& key) -> void;

  auto reset_trigger(const utility::hashed_string& key) -> void;

  auto play(const utility::hashed_string& state_name, const bool instant = false, const std::float_t cross_fade = 0.0f) -> void;

  auto update(const std::float_t delta_time) -> void;

  auto evaluate_locals(const skeleton& skeleton) -> std::vector<bone_transform>;

  auto evaluate_pose(const skeleton& skeleton, const std::vector<bone_transform>& locals) -> std::vector<math::matrix4x4>;

  auto current_state_name() const -> const utility::hashed_string&;

  auto is_in_transition() const -> bool;

  auto bool_parameter(const utility::hashed_string& key) const -> std::optional<bool>;

  auto float_parameter(const utility::hashed_string& key) const -> std::optional<std::float_t>;

  auto trigger_parameter(const utility::hashed_string& key) const -> std::optional<bool>;

private:

  auto _start_playback(const state& target_state, const bool instant, const std::float_t cross_fade) -> void;

  auto _wrap_state_time(const state& state, std::float_t time) const -> std::float_t;

  auto _has_valid_clip(const state& state) const -> bool;

private:

  std::unordered_map<utility::hashed_string, state> _state_map;
  std::vector<transition> _transitions;
  parameters _parameters{};

  state _current_state{};
  state _next_state{};
  std::float_t _current_state_time{0.0f};
  std::float_t _next_state_time{0.0f};

  bool _is_in_transition{false};
  std::float_t _cross_fade_duration{0.0f};
  std::float_t _cross_fade_elapsed{0.0f};

}; // class animator 

} // namespace sbx::animations

#endif // LIBSBX_ANIMATIONS_ANIMATOR_HPP_
