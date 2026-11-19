// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_ANIMATION_GRAPH_HPP_
#define LIBSBX_ASSETS_ANIMATION_GRAPH_HPP_

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/bounded.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>

namespace sbx::assets {

struct animation_trigger {
  bool set{false};
}; // struct animation_trigger

using animation_parameter_value = std::variant<std::float_t, bool, std::int32_t, animation_trigger>;

struct animation_parameter {
  std::string name{};
  animation_parameter_value default_value{0.0f};
}; // struct animation_parameter

/** @brief How a blend motion weighs its children. */
enum class animation_blend_type : std::uint8_t {
  linear_1d, // by one float parameter between neighbouring thresholds
  freeform_directional_2d, // by two float parameters, children placed by direction and speed (walk/run in every direction)
  freeform_cartesian_2d, // by two float parameters, children placed on independent axes
  direct // each child weighted by its own float parameter
}; // enum class animation_blend_type

enum class animation_motion_kind : std::uint8_t {
  clip,
  blend
}; // enum class animation_motion_kind

// Children past this many in one blend are ignored.
inline constexpr auto max_blend_children = std::size_t{32u};

/**
 * @brief What a state plays: a clip, or a blend of child motions (Unity's blend tree), nested to any depth.
 *
 * Every clip in a state's tree plays at the same normalized time, so cycles of different lengths (walk, run) stay in step.
 */
struct animation_motion {
  animation_motion_kind kind{animation_motion_kind::clip};

  // kind == clip: resolved by name against the animated mesh's clips.
  std::string clip_name{};

  // kind == blend.
  animation_blend_type blend_type{animation_blend_type::linear_1d};
  std::string parameter_x{};
  std::string parameter_y{};
  std::vector<animation_motion> children{};

  // Where this motion sits in its parent blend: its threshold (linear_1d), position (2D), or weight parameter (direct).
  std::float_t threshold{0.0f};
  math::vector2 position{0.0f, 0.0f};
  std::string direct_parameter{};
}; // struct animation_motion

struct animation_state {
  std::uint32_t id{0u};
  std::string name{};
  animation_motion motion{};
  std::float_t speed{1.0f};
  std::string speed_parameter{}; // a float parameter multiplying speed, empty for none
  bool loop{true};
  math::vector2 editor_position{0.0f, 0.0f};
}; // struct animation_state

enum class animation_condition_comparator : std::uint8_t {
  equals,
  not_equals,
  greater,
  greater_or_equal,
  less,
  less_or_equal
}; // enum class animation_condition_comparator

struct animation_condition {
  std::string parameter_name{};
  animation_condition_comparator comparator{animation_condition_comparator::equals};
  animation_parameter_value expected{0.0f};
}; // struct animation_condition

struct animation_transition {
  std::optional<std::uint32_t> from_state{};
  std::uint32_t to_state{0u};
  std::float_t duration{0.25f};
  bool has_exit_time{false};
  math::bounded<std::float_t, 0.0f, 1.0f> exit_time{1.0f};
  std::vector<animation_condition> conditions{};
}; // struct animation_transition

enum class animation_layer_blend_mode : std::uint8_t {
  override, // replaces the layers below, by weight
  additive // adds its motion's difference from the motion's first frame onto the layers below
}; // enum class animation_layer_blend_mode

/**
 * @brief One state machine of a graph; layers above the first blend over the ones below, limited to the joints in their mask.
 *
 * Every layer reads the graph's shared parameters.
 */
struct animation_layer {
  std::string name{"Base Layer"};
  std::vector<animation_state> states{};
  std::vector<animation_transition> transitions{};
  std::uint32_t entry_state_id{0u};
  std::float_t weight{1.0f}; // the default weight; the base layer always has full weight
  animation_layer_blend_mode blend_mode{animation_layer_blend_mode::override};
  std::vector<std::string> mask{}; // names of the joints this layer drives, matched against the animated skeleton; empty drives every joint

  /**
   * @brief An id no state of this layer uses yet.
   *
   * @return One past the highest state id.
   */
  [[nodiscard]] auto next_state_id() const noexcept -> std::uint32_t {
    auto next = std::uint32_t{0u};

    for (const auto& state : states) {
      next = std::max(next, state.id + 1u);
    }

    return next;
  }
}; // struct animation_layer

class animation_graph final : public loadable {

  friend class asset_residency;

public:

  struct create_info {
    std::string name{"animation_graph"};
    std::vector<animation_parameter> parameters{};
    std::vector<animation_layer> layers{}; // the first is the base layer
  }; // struct create_info

  animation_graph() = default;

  explicit animation_graph(const create_info& create_info)
  : _name{create_info.name},
    _parameters{create_info.parameters},
    _layers{create_info.layers} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return !_layers.empty() && !_layers.front().states.empty();
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& {
    return _name;
  }

  [[nodiscard]] auto parameters() const noexcept -> const std::vector<animation_parameter>& {
    return _parameters;
  }

  [[nodiscard]] auto layers() const noexcept -> const std::vector<animation_layer>& {
    return _layers;
  }

private:

  std::string _name{"animation_graph"};
  std::vector<animation_parameter> _parameters{};
  std::vector<animation_layer> _layers{};
  math::uuid _id{math::uuid::nil()};

}; // class animation_graph

using animation_graph_handle = asset_handle<animation_graph>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ANIMATION_GRAPH_HPP_
