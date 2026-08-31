// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/narrowphase.hpp
 *
 * @brief Per-pair narrowphase dispatch: closed forms for the cheap/common pairs (sphere-*,
 * capsule-capsule), SAT with face clipping for box-box (the primary stacking/resting case), and
 * generic GJK/EPA for everything else.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_NARROWPHASE_HPP_
#define LIBSBX_PHYSICS_NARROWPHASE_HPP_

#include <optional>

#include <libsbx/scenes/node.hpp>

#include <libsbx/physics/contact.hpp>

namespace sbx::physics {

/**
 * @brief Runs narrowphase for a broadphase-candidate pair, both of which must carry a
 * shape_collider. Returns nullopt if the shapes don't actually overlap.
 */
[[nodiscard]] auto generate_contact(const sbx::scenes::node& node_a, const sbx::scenes::node& node_b) -> std::optional<contact_manifold>;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_NARROWPHASE_HPP_
