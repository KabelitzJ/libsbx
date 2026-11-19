// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ANIMATION_POSE_EVALUATION_HPP_
#define LIBSBX_ANIMATION_POSE_EVALUATION_HPP_

#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/skeleton.hpp>

#include <libsbx/scenes/components.hpp>

namespace sbx::animation {

/** @brief An animation event a clip played past this frame, on its way to the node's scripts. */
struct fired_animation_event {
  math::uuid node{math::uuid::nil()}; // filled in by animation_module
  std::string name{};
  std::float_t float_parameter{0.0f};
  std::int32_t int_parameter{0};
  std::string string_parameter{};
  std::string clip_name{};
  std::uint32_t layer{0u};
  std::float_t weight{0.0f}; // the clip's weight when it fired
}; // struct fired_animation_event

/** @brief One instance's root motion while it's evaluated: which joint carries it, how it sits in the mesh, and the motion gathered so far this frame. */
struct root_motion_context {
  std::int32_t joint{-1}; // -1: this instance has no root motion this frame
  math::matrix4x4 parent{math::matrix4x4::identity}; // the root joint's parent in mesh space, bind pose
  math::quaternion parent_rotation{math::quaternion::identity};
  math::vector3 bind_translation{0.0f, 0.0f, 0.0f};
  math::quaternion bind_rotation{math::quaternion::identity};
  bool extract_vertical{false};
  bool extract_rotation{true};
  math::vector3 delta_position{0.0f, 0.0f, 0.0f}; // mesh space
  std::float_t delta_yaw{0.0f}; // radians about the mesh's up axis
}; // struct root_motion_context

/** @brief Where a downward ground probe for foot placement landed, world space. */
struct ground_hit {
  math::vector3 point{0.0f, 0.0f, 0.0f};
  math::vector3 normal{0.0f, 1.0f, 0.0f};
}; // struct ground_hit

/** @brief Casts straight down from a world-space point for up to a distance against the given layers; must be safe to call from several threads at once. */
using ground_query = std::function<std::optional<ground_hit>(const math::vector3& origin, std::float_t distance, const scenes::layer_mask& mask)>;

/** @brief Per-thread working storage for evaluate_skeleton_pose; kept across frames for its capacity. */
struct pose_scratch {
  std::vector<math::vector3> translations{};
  std::vector<math::quaternion> rotations{};
  std::vector<math::vector3> scales{};
  std::vector<math::vector3> target_translations{};
  std::vector<math::quaternion> target_rotations{};
  std::vector<math::vector3> target_scales{};
  std::vector<math::matrix4x4> locals{};

  // One blend-tree leaf at a time, accumulated into the state's pose.
  std::vector<math::vector3> leaf_translations{};
  std::vector<math::quaternion> leaf_rotations{};
  std::vector<math::vector3> leaf_scales{};

  // A layer above the base, and the first-frame pose an additive layer measures against.
  std::vector<math::vector3> layer_translations{};
  std::vector<math::quaternion> layer_rotations{};
  std::vector<math::vector3> layer_scales{};
  std::vector<math::vector3> reference_translations{};
  std::vector<math::quaternion> reference_rotations{};
  std::vector<math::vector3> reference_scales{};

  // Per-leaf weights of the current and the transition target state.
  std::vector<std::float_t> leaf_weights{};
  std::vector<std::float_t> target_leaf_weights{};

  // Events fired by the instances this scratch evaluated, in order; the caller drains it.
  std::vector<fired_animation_event> fired_events{};

  // The instance being evaluated.
  root_motion_context root_motion{};

  // Set by the caller before each instance: its node's world matrix, and the ground probe foot placement uses (empty disables it).
  math::matrix4x4 world{math::matrix4x4::identity};
  ground_query ground{};

  // IK's view of the pose in mesh space, recomputed as joints move.
  std::vector<math::matrix4x4> ik_matrices{};
  std::vector<math::quaternion> ik_rotations{};
}; // struct pose_scratch

/**
 * @brief Grows an animator's layers to its graph's, each new one starting in its entry state at its default weight, and drops layers the graph no longer has.
 *
 * Evaluation does this every frame; call it before touching animator.layers from elsewhere (e.g. scripts setting a weight before the first evaluation).
 *
 * @param graph The animator's graph.
 * @param animator The animator.
 */
auto reconcile_animator_layers(const assets::animation_graph& graph, scenes::animator& animator) -> void;

/**
 * @brief The weights a blend motion gives its first @p weights .size() children for a set of parameter values.
 *
 * They sum to 1, except for a direct blend whose parameters are all zero (all weights zero).
 *
 * @param motion A blend motion.
 * @param parameters Parameter values by name; ints count as floats and bools as 0 or 1, missing ones as 0.
 * @param weights Receives one weight per child; at most assets::max_blend_children.
 */
auto blend_child_weights(const assets::animation_motion& motion, const std::vector<std::pair<std::string, assets::animation_parameter_value>>& parameters, std::span<std::float_t> weights) -> void;

/**
 * @brief Advances every layer's state machine and samples the pose into @p pose: the base layer, then each layer above it blended in by weight and mask (override or additive), each state's motion tree blended and crossfades applied.
 *
 * With apply_root_motion, the base layer's root joint motion is taken out of the pose and left in the animator's root_motion_delta_position/rotation for the caller to apply.
 * IK runs last: foot placement (with scratch.ground), the chains' goals, then look-at, all converted into mesh space through scratch.world.
 *
 * Touches only its own pose, animator and scratch, so instances evaluate in parallel.
 *
 * @param skeleton The skeleton being posed.
 * @param renderer The instance's mesh_renderer, for resolving clip names.
 * @param animator The animator, or null for the bind pose.
 * @param pose Receives joint world and skinning matrices.
 * @param delta_time The time step.
 * @param scratch Working storage owned by the calling thread; receives the events fired (node left nil).
 */
auto evaluate_skeleton_pose(const assets::skeleton& skeleton, const scenes::mesh_renderer& renderer, memory::observer_ptr<scenes::animator> animator, scenes::skeleton_pose& pose, std::float_t delta_time, pose_scratch& scratch) -> void;

} // namespace sbx::animation

#endif // LIBSBX_ANIMATION_POSE_EVALUATION_HPP_
