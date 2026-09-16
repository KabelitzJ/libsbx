// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_state.hpp>

#include <algorithm>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

namespace editor {

auto editor_state::selected_node(sbx::scenes::scene& scene) const -> sbx::scenes::node {
  if (const auto* selected = std::get_if<node_selection>(&current_selection); selected != nullptr && !selected->ids.empty()) {
    if (auto node = scene.find(selected->ids.back()); node.is_valid()) {
      return node;
    }
  }

  return sbx::scenes::node{};
}

auto editor_state::is_node_selected(const sbx::scenes::node& node) const noexcept -> bool {
  const auto* selected = std::get_if<node_selection>(&current_selection);

  if (selected == nullptr || !node.is_valid()) {
    return false;
  }

  return std::find(selected->ids.begin(), selected->ids.end(), node.id()) != selected->ids.end();
}

auto editor_state::select_node(const sbx::scenes::node& node) -> void {
  current_selection = node_selection{{node.id()}, node.id()};
}

auto editor_state::toggle_node_selection(const sbx::scenes::node& node) -> void {
  auto* selected = std::get_if<node_selection>(&current_selection);
  auto updated = selected != nullptr ? std::move(*selected) : node_selection{};

  if (const auto entry = std::find(updated.ids.begin(), updated.ids.end(), node.id()); entry != updated.ids.end()) {
    updated.ids.erase(entry);
  } else {
    updated.ids.push_back(node.id());
  }

  updated.range_anchor = node.id();

  if (updated.ids.empty()) {
    current_selection = empty_selection{};
  } else {
    current_selection = std::move(updated);
  }
}

auto editor_state::add_node_to_selection(const sbx::scenes::node& node) -> void {
  auto* selected = std::get_if<node_selection>(&current_selection);
  auto updated = selected != nullptr ? std::move(*selected) : node_selection{};

  if (std::find(updated.ids.begin(), updated.ids.end(), node.id()) == updated.ids.end()) {
    updated.ids.push_back(node.id());
  }

  updated.range_anchor = node.id();

  current_selection = std::move(updated);
}

auto editor_state::set_node_selection(std::vector<sbx::math::uuid> ids) -> void {
  if (ids.empty()) {
    current_selection = empty_selection{};
    return;
  }

  const auto anchor = node_selection_anchor();

  current_selection = node_selection{std::move(ids), anchor};
}

auto editor_state::selected_node_ids() const -> const std::vector<sbx::math::uuid>& {
  static const auto empty = std::vector<sbx::math::uuid>{};

  const auto* selected = std::get_if<node_selection>(&current_selection);

  return selected != nullptr ? selected->ids : empty;
}

auto editor_state::node_selection_anchor() const -> sbx::math::uuid {
  const auto* selected = std::get_if<node_selection>(&current_selection);

  return selected != nullptr ? selected->range_anchor : sbx::math::uuid::nil();
}

auto editor_state::selected_node_count() const -> std::size_t {
  return selected_node_ids().size();
}

auto editor_state::select_asset(sbx::math::uuid id, std::filesystem::path path, asset_kind kind) -> void {
  current_selection = asset_selection{id, std::move(path), kind};
}

auto editor_state::clear_selection() -> void {
  current_selection = empty_selection{};
}

} // namespace editor
