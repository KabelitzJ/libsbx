// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/physics_debug.hpp
 *
 * @brief Physics debug wireframes submitted to render::debug_draw; physics_module::late_update() is the only caller.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_PHYSICS_DEBUG_HPP_
#define LIBSBX_PHYSICS_PHYSICS_DEBUG_HPP_

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/render/debug/debug_draw.hpp>

#include <span>

#include <libsbx/physics/shapes.hpp>
#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>
#include <libsbx/physics/nav/navmesh.hpp>
#include <libsbx/physics/nav/navmesh_query.hpp>

namespace sbx::physics {

/** @brief Which debug layers physics_module submits; only colliders are on by default. */
struct debug_draw_flags {
  bool colliders{false};
  bool broadphase{false};
  bool contacts{false};
  bool navmesh{false};
  bool nav_agents{false};
}; // struct debug_draw_flags

/**
 * @brief Box2D/Bullet-style colors: green dynamic, blue kinematic, grey static, darker when asleep.
 *
 * @param type The body type.
 * @param is_sleeping Whether the body is asleep.
 *
 * @return The color.
 */
[[nodiscard]] auto debug_color_for(body_type type, bool is_sleeping) -> math::color;

/**
 * @brief Appends @p shape's wireframe to @p debug_draw.
 *
 * Scale is applied to the shape's dimensions rather than baked into @p matrix, since the sphere/cylinder/capsule helpers normalize the matrix axes.
 * Non-uniformly scaled spheres, cylinders and capsules draw with scale.x as a representative (cosmetic only). Triangles draw nothing; hulls draw their faces, or crosses at their points when they have none.
 *
 * @param debug_draw The accumulator.
 * @param shape The shape.
 * @param matrix The collider's world rotation and translation.
 * @param scale The pose's per-axis scale.
 * @param color The line color.
 */
auto draw_convex_shape(render::debug_draw& debug_draw, const convex_shape& shape, const math::matrix4x4& matrix, const math::vector3& scale, const math::color& color) -> void;

/**
 * @brief Draws every polygon edge, with unlinked edges (walls or failed links) in @p boundary_color so seams that look continuous but aren't pathable stand out.
 *
 * @param debug_draw The accumulator.
 * @param mesh The navmesh.
 * @param color The edge color.
 * @param boundary_color The unlinked edge color.
 */
auto draw_navmesh(render::debug_draw& debug_draw, const navmesh& mesh, const math::color& color, const math::color& boundary_color) -> void;

auto draw_nav_path(render::debug_draw& debug_draw, std::span<const straight_path_point> path, const math::color& color) -> void;

auto draw_nav_agent(render::debug_draw& debug_draw, const nav_agent& agent, const math::vector3& position, const math::color& color) -> void;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_PHYSICS_DEBUG_HPP_
