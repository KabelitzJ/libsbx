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
 * @brief One standalone collapsible per attached script, same shape as the component sections in
 * inspector_component_sections.hpp, but called once per script_component entry rather than guarded
 * by has_component<T>() -- see inspector_panel::_draw_node_properties. Title assumes the
 * "<ClassName>.cs" file-name convention "New Script" generates.
 */
auto draw_script_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::scenes::script_entry& entry, std::optional<std::string>& pending_removal) -> void;

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_SCRIPT_SECTION_HPP_
