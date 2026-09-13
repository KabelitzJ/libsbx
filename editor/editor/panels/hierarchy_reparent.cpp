// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/hierarchy_panel.hpp>

#include <algorithm>
#include <memory>
#include <vector>

#include <fmt/format.h>

#include <libsbx/scenes/components.hpp>

#include <editor/commands/composite_command.hpp>
#include <editor/commands/scene_commands.hpp>

namespace editor {

auto hierarchy_panel::_try_reparent(editor_state& state, sbx::scenes::scene& scene, sbx::math::uuid payload_id, std::optional<sbx::math::uuid> new_parent_id, std::size_t new_index) -> void {
  const auto& selected = state.selected_node_ids();

  const auto has_selected = (selected.size() > 1u && std::find(selected.begin(), selected.end(), payload_id) != selected.end());

  auto dragged_ids = has_selected ? selected : std::vector<sbx::math::uuid>{payload_id};

  if (new_parent_id) {
    if (std::find(dragged_ids.begin(), dragged_ids.end(), *new_parent_id) != dragged_ids.end()) {
      return;
    }

    auto ancestor = scene.find(*new_parent_id);

    while (ancestor.is_valid()) {
      if (std::find(dragged_ids.begin(), dragged_ids.end(), ancestor.id()) != dragged_ids.end()) {
        return;
      }

      auto next = scene.node_of(ancestor.get_component<sbx::scenes::relationship>().parent);

      if (!next.has_component<sbx::scenes::id>()) {
        break;
      }

      ancestor = next;
    }
  }

  _pending_reparent = pending_reparent{std::move(dragged_ids), new_parent_id, new_index};
}

auto hierarchy_panel::_current_parent_id(sbx::scenes::scene& scene, sbx::math::uuid id) const -> std::optional<sbx::math::uuid> {
  auto node = scene.find(id);

  if (!node.is_valid()) {
    return std::nullopt;
  }

  auto parent = scene.node_of(node.get_component<sbx::scenes::relationship>().parent);

  if (!parent.has_component<sbx::scenes::id>()) {
    return std::nullopt;
  }

  return parent.id();
}

auto hierarchy_panel::_filter_to_selection_roots(sbx::scenes::scene& scene, const std::vector<sbx::math::uuid>& ids) const -> std::vector<sbx::math::uuid> {
  auto roots = std::vector<sbx::math::uuid>{};

  for (const auto id : ids) {
    const auto parent_id = _current_parent_id(scene, id);

    if (!parent_id || std::find(ids.begin(), ids.end(), *parent_id) == ids.end()) {
      roots.push_back(id);
    }
  }

  return roots;
}

auto hierarchy_panel::_apply_pending_reparent(editor_state& state, sbx::scenes::scene& scene) -> void {
  if (!_pending_reparent) {
    return;
  }

  auto ordered = _filter_to_selection_roots(scene, _pending_reparent->dragged_ids);

  std::stable_sort(ordered.begin(), ordered.end(), [this](sbx::math::uuid a, sbx::math::uuid b) {
    const auto index_of = [this](sbx::math::uuid id) {
      const auto entry = std::find(_visible_row_order.begin(), _visible_row_order.end(), id);
      return entry == _visible_row_order.end() ? _visible_row_order.size() : static_cast<std::size_t>(entry - _visible_row_order.begin());
    };

    return index_of(a) < index_of(b);
  });

  if (ordered.size() == 1u) {
    if (auto target = scene.find(ordered.front()); target.is_valid()) {
      state.push_command(scene, std::make_unique<reparent_node_command>(scene, target, _pending_reparent->new_parent_id, _pending_reparent->new_index));
    }
  } else if (ordered.size() > 1u) {
    auto sub_commands = std::vector<std::unique_ptr<command>>{};
    auto foreign_count = std::size_t{0u};

    for (const auto id : ordered) {
      auto target = scene.find(id);

      if (!target.is_valid()) {
        continue;
      }

      const auto was_already_sibling = _current_parent_id(scene, id) == _pending_reparent->new_parent_id;

      sub_commands.push_back(std::make_unique<reparent_node_command>(scene, target, _pending_reparent->new_parent_id, _pending_reparent->new_index + foreign_count));

      if (!was_already_sibling) {
        foreign_count += 1u;
      }
    }

    if (!sub_commands.empty()) {
      state.push_command(scene, std::make_unique<composite_command>(std::move(sub_commands), fmt::format("Move {} Nodes", sub_commands.size())));
    }
  }

  _pending_reparent.reset();
}

} // namespace editor
