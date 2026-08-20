// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_state.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

namespace editor {

auto editor_state::selected_node(sbx::scenes::scene& scene) const -> sbx::scenes::node {
  if (const auto* selected = std::get_if<node_selection>(&current_selection); selected != nullptr) {
    if (auto node = scene.node_of(selected->entity); node.is_valid()) {
      return node;
    }
  }

  return sbx::scenes::node{};
}

auto editor_state::is_node_selected(sbx::ecs::entity entity) const noexcept -> bool {
  const auto* selected = std::get_if<node_selection>(&current_selection);
  return selected != nullptr && selected->entity == entity;
}

auto editor_state::select_node(sbx::ecs::entity entity) -> void {
  current_selection = node_selection{entity};
}

auto editor_state::select_asset(sbx::math::uuid id, std::filesystem::path path, asset_kind kind) -> void {
  current_selection = asset_selection{id, std::move(path), kind};
}

auto editor_state::clear_selection() -> void {
  current_selection = empty_selection{};
}

} // namespace editor
