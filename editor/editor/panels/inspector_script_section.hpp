// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_INSPECTOR_SCRIPT_SECTION_HPP_
#define EDITOR_PANELS_INSPECTOR_SCRIPT_SECTION_HPP_

#include <optional>
#include <string>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief One collapsible section per attached script; the title assumes the "<ClassName>.cs" naming that New Script generates.
 *
 * @param state The editor state.
 * @param target The scene.
 * @param node The node.
 * @param entry The script entry.
 * @param pending_removal Receives the class name to detach after the caller's loop.
 */
auto draw_script_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::scenes::script_entry& entry, std::optional<std::string>& pending_removal) -> void;

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_SCRIPT_SECTION_HPP_
