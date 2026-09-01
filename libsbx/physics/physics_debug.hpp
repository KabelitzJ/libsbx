// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/physics_debug.hpp
 *
 * @brief Turns physics state into calls against the generic render::debug_draw accumulator (see
 * libsbx/render/debug/debug_draw.hpp). physics_module::late_update() is the only caller -- it owns
 * the broadphase-tree/manifold iteration (private state), and defers to draw_convex_shape()/
 * debug_color_for() here for the parts that don't need that access.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_PHYSICS_DEBUG_HPP_
#define LIBSBX_PHYSICS_PHYSICS_DEBUG_HPP_

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/render/debug/debug_draw.hpp>

#include <libsbx/physics/shapes.hpp>
#include <libsbx/physics/rigidbody.hpp>

namespace sbx::physics {

/**
 * @brief Which debug layers physics_module::late_update() submits into render::debug_draw. All default
 * off except colliders, matching grid_pass's "off unless an app opts in" convention -- see
 * physics_module::debug_draw_flags()/set_debug_draw_flags().
 */
struct debug_draw_flags {
  bool colliders{false};
  bool broadphase{false};
  bool contacts{false};
}; // struct debug_draw_flags

/**
 * @brief Box2D/Bullet-style convention: green = awake dynamic, blue = kinematic, grey = static,
 * darker when asleep.
 */
[[nodiscard]] auto debug_color_for(body_type type, bool is_sleeping) -> math::color;

/**
 * @brief Dispatches on @p shape's active alternative and appends its wireframe into @p debug_draw.
 * @p matrix is the collider's full world pose (the body's transform composed with the collider's
 * own local offset/rotation) -- see shape_collider::offset/rotation. No-op for `triangle` (a
 * mesh_collider narrowphase candidate, never itself drawn); `convex_hull` draws its actual hull
 * faces as a wireframe, falling back to its bare point set as small crosses if it has none (a
 * degenerate source mesh -- see quickhull.hpp's compute_convex_hull).
 */
auto draw_convex_shape(render::debug_draw& debug_draw, const convex_shape& shape, const math::matrix4x4& matrix, const math::color& color) -> void;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_PHYSICS_DEBUG_HPP_
