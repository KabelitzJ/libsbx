// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PROBE_BAKING_HPP_
#define EDITOR_PROBE_BAKING_HPP_

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Captures a reflection probe's surroundings, writes them as an .hdr under the assets' baked/<scene>/ folder, imports it as an environment map and assigns it to the probe (undoable).
 *
 * The capture renders over the next few frames; the probe is looked up again by id when it completes, so deleting it meanwhile only skips the assignment.
 *
 * @param state The editor state the assignment's command is pushed to.
 * @param scene The scene the probe is in.
 * @param node A node with a reflection_probe.
 */
auto bake_reflection_probe(editor_state& state, sbx::scenes::scene& scene, sbx::scenes::node node) -> void;

/**
 * @brief Bakes every active reflection probe of a scene, one after another.
 *
 * @param state The editor state the assignments' commands are pushed to.
 * @param scene The scene.
 */
auto bake_all_reflection_probes(editor_state& state, sbx::scenes::scene& scene) -> void;

} // namespace editor

#endif // EDITOR_PROBE_BAKING_HPP_
