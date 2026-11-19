// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ANIMATION_ANIMATION_MODULE_HPP_
#define LIBSBX_ANIMATION_ANIMATION_MODULE_HPP_

#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/signals/signal.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/async/async_module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/animation/pose_evaluation.hpp>

namespace sbx::animation {

/** @brief One animated node's root motion for the frame, offered to its scripts before the engine applies it. */
struct root_motion_event {
  math::uuid node{math::uuid::nil()};
  math::vector3 delta_position{0.0f, 0.0f, 0.0f}; // in the node's own space
  math::quaternion delta_rotation{math::quaternion::identity}; // about the node's up axis
  bool handled{false}; // set by a listener that applied the motion itself (OnAnimatorMove)
}; // struct root_motion_event

/**
 * @brief Advances every animator's state machine and poses every skinned mesh of the active scene, once per frame.
 *
 * Runs in post_update: after scripts' Update, so parameters they set apply this frame, and before the world transforms are refreshed in late_update.
 * Instances evaluate in parallel on the async pool; the renderer only reads the resulting skeleton_pose.
 */
class animation_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<async::async_module, assets::assets_module, scenes::scenes_module>;

  auto post_update() -> void;

  /** @brief Fired for every animation event a clip played past, after all poses of the frame are evaluated, in instance order and then time order. */
  [[nodiscard]] auto on_event() -> signals::signal<const fired_animation_event&>& {
    return _on_event;
  }

  /**
   * @brief Fired for every node whose animator has IK chains or a look-at joint, before the frame's poses are evaluated, so scripts can set goals (OnAnimatorIK).
   *
   * @return The signal.
   */
  [[nodiscard]] auto on_animator_ik() -> signals::signal<const math::uuid&>& {
    return _on_animator_ik;
  }

  /**
   * @brief Fired after the events for every node with apply_root_motion; unless a listener marks it handled, the motion then moves the node, or its character_controller on the next fixed step.
   *
   * @return The signal.
   */
  [[nodiscard]] auto on_animator_move() -> signals::signal<root_motion_event&>& {
    return _on_animator_move;
  }

private:

  /**
   * @brief Moves a node by its root motion: its transform, or its character controller's pending motion plus the turn.
   *
   * @param motion The node's root motion.
   */
  auto _apply_root_motion(const root_motion_event& motion) -> void;

  struct skinned_entry {
    math::uuid node{math::uuid::nil()};
    memory::observer_ptr<const scenes::mesh_renderer> renderer{};
    memory::observer_ptr<scenes::skeleton_pose> pose{};
    memory::observer_ptr<scenes::animator> animator{};
    math::matrix4x4 world{math::matrix4x4::identity}; // last frame's, IK's bridge between world-space goals and the mesh
  }; // struct skinned_entry

  std::vector<skinned_entry> _entries{};

  // One per parallel chunk.
  std::vector<pose_scratch> _scratch{};

  signals::signal<const fired_animation_event&> _on_event{};
  signals::signal<root_motion_event&> _on_animator_move{};
  signals::signal<const math::uuid&> _on_animator_ik{};

  std::vector<math::uuid> _ik_nodes{};

  std::vector<root_motion_event> _root_motions{};

}; // class animation_module

} // namespace sbx::animation

#endif // LIBSBX_ANIMATION_ANIMATION_MODULE_HPP_
