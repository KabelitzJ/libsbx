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

#include <libsbx/assets/asset_handle.hpp>

namespace sbx::assets {

/**
 * @brief A fired-but-not-yet-consumed pulse (Unity's "Trigger" parameter kind) -- a dedicated
 * type rather than a bool, so a trigger and a persistent bool aren't the same std::variant
 * alternative. Consumed (reset to false) once per frame by the animator evaluator, whether or
 * not it caused a transition.
 */
struct animation_trigger {
  bool set{false};
}; // struct animation_trigger

/**
 * @brief An animation_parameter's (or animation_condition's) value -- the parameter's type *is*
 * whichever alternative is active, so callers std::get_if/std::visit it instead of switching on
 * a separate type enum. Shared as-is between an animation_parameter's default and an animator
 * instance's live value (scenes::animator::parameters).
 */
using animation_parameter_value = std::variant<std::float_t, bool, std::int32_t, animation_trigger>;

struct animation_parameter {
  std::string name{};
  animation_parameter_value default_value{0.0f};
}; // struct animation_parameter

/**
 * @brief One state's clip binding. clip_name is resolved at evaluation time by name against
 * whichever mesh_renderer's mesh the graph is driving (scenes::animator is on the same node) --
 * clips aren't independent assets, just a mesh's cooked side effects (see assets::animation_clip's
 * doc comment), so a graph is only meaningful when applied to a mesh whose clip names match.
 */
struct animation_state {
  std::uint32_t id{0u}; // stable, not a vector index -- survives reordering/removal
  std::string name{};
  std::string clip_name{};
  std::float_t speed{1.0f};
  bool loop{true};
  math::vector2 editor_position{0.0f, 0.0f}; // unused until the visual graph editor lands; carried now so that editor doesn't need an asset-format migration
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
  animation_condition_comparator comparator{animation_condition_comparator::equals}; // ignored when the named parameter is an animation_trigger
  animation_parameter_value expected{0.0f}; // trigger alternative unused here
}; // struct animation_condition

/** @brief One edge of the state machine. Every listed condition must pass (ANDed) for the transition to be taken. */
struct animation_transition {
  std::optional<std::uint32_t> from_state{}; // nullopt = "Any State"
  std::uint32_t to_state{0u};
  std::float_t duration{0.25f}; // crossfade seconds
  bool has_exit_time{false};
  std::float_t exit_time{1.0f}; // normalized [0, 1] against the source state's clip duration
  std::vector<animation_condition> conditions{}; // ANDed
}; // struct animation_transition

/**
 * @brief A state machine over a set of named animation states: which clip each state plays, and
 * the parameter-gated transitions (with crossfade duration) between them. Evaluated per-instance
 * by scenes::animator + render::scene_renderer_module -- this class only holds the authored
 * graph, not any entity's live state/parameter values.
 *
 * Mirrors particle_effect's asset shape: a create_info residency builds the live object from,
 * plain-data members, id()/is_valid().
 */
class animation_graph final {

  friend class asset_residency;

public:

  struct create_info {
    std::string name{"animation_graph"};
    std::vector<animation_parameter> parameters{};
    std::vector<animation_state> states{};
    std::vector<animation_transition> transitions{};
    std::uint32_t entry_state_id{0u};
  }; // struct create_info

  animation_graph() = default;

  explicit animation_graph(const create_info& create_info)
  : _name{create_info.name},
    _parameters{create_info.parameters},
    _states{create_info.states},
    _transitions{create_info.transitions},
    _entry_state_id{create_info.entry_state_id} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return !_states.empty();
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

  [[nodiscard]] auto states() const noexcept -> const std::vector<animation_state>& {
    return _states;
  }

  [[nodiscard]] auto transitions() const noexcept -> const std::vector<animation_transition>& {
    return _transitions;
  }

  [[nodiscard]] auto entry_state_id() const noexcept -> std::uint32_t {
    return _entry_state_id;
  }

  /** @brief One past the highest state id in use -- the editor's convenience for minting a fresh id when adding a state. */
  [[nodiscard]] auto next_state_id() const noexcept -> std::uint32_t {
    auto next = std::uint32_t{0u};

    for (const auto& state : _states) {
      next = std::max(next, state.id + 1u);
    }

    return next;
  }

private:

  std::string _name{"animation_graph"};
  std::vector<animation_parameter> _parameters{};
  std::vector<animation_state> _states{};
  std::vector<animation_transition> _transitions{};
  std::uint32_t _entry_state_id{0u};
  math::uuid _id{math::uuid::nil()};

}; // class animation_graph

using animation_graph_handle = asset_handle<animation_graph>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_ANIMATION_GRAPH_HPP_
