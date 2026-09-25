// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/canvas/layout_resolve.hpp>

#include <algorithm>
#include <cmath>

#include <libsbx/scenes/components.hpp>

namespace sbx::canvas {

auto layout_resolver::child_axis_size(scenes::scene& scene, scenes::node child, bool horizontal) -> axis_size {
  const auto& rect = child.get_component<rect_transform>();
  const auto natural = horizontal ? rect.size_delta.x() : rect.size_delta.y();

  auto result = axis_size{0.0f, natural, 0.0f};

  // A nested layout group wants the size of its content -- without this, a box stacking rows
  // reports its (unsized) rect and the parent group squashes it to nothing.
  if (child.has_component<horizontal_layout_group>() || child.has_component<vertical_layout_group>()) {
    const auto preferred = compute_preferred_size(scene, child, false);
    const auto minimum = compute_preferred_size(scene, child, true);

    result.preferred = horizontal ? preferred.x() : preferred.y();
    result.min = horizontal ? minimum.x() : minimum.y();
  }

  if (child.has_component<layout_element>()) {
    const auto& element = child.get_component<layout_element>();
    const auto min = horizontal ? element.min_width : element.min_height;
    const auto preferred = horizontal ? element.preferred_width : element.preferred_height;
    const auto flexible = horizontal ? element.flexible_width : element.flexible_height;

    if (min >= 0.0f) {
      result.min = min;
    }

    if (preferred >= 0.0f) {
      result.preferred = preferred;
    }

    if (flexible >= 0.0f) {
      result.flexible = flexible;
    }
  }

  return result;
}

auto is_layout_child(scenes::node child) -> bool {
  // An inactive child takes no space, same as it draws nothing (canvas_module::_visit).
  if (!child.has_component<rect_transform>() || child.has_component<scenes::inactive>()) {
    return false;
  }

  if (child.has_component<layout_element>() && child.get_component<layout_element>().ignore_layout) {
    return false;
  }

  return true;
}

auto alignment_component(layout_alignment alignment, bool horizontal) -> std::int32_t {
  const auto ordinal = static_cast<std::int32_t>(alignment);

  return horizontal ? (ordinal % 3) : (ordinal / 3);
}

auto alignment_offset(std::float_t used, std::float_t available, std::int32_t component) -> std::float_t {
  const auto extra = std::max(0.0f, available - used);

  if (component == 1) {
    return extra * 0.5f;
  }

  if (component == 2) {
    return extra;
  }

  return 0.0f;
}

auto layout_resolver::compute_preferred_size(scenes::scene& scene, scenes::node node, bool use_min) -> math::vector2 {
  if (!node.has_component<scenes::relationship>()) {
    return math::vector2{0.0f, 0.0f};
  }

  const auto is_horizontal_group = node.has_component<horizontal_layout_group>();
  const auto is_vertical_group = node.has_component<vertical_layout_group>();

  auto spacing = 0.0f;
  auto padding = math::vector4{0.0f, 0.0f, 0.0f, 0.0f};

  if (is_horizontal_group) {
    const auto& group = node.get_component<horizontal_layout_group>();
    spacing = group.spacing;
    padding = group.padding;
  } else if (is_vertical_group) {
    const auto& group = node.get_component<vertical_layout_group>();
    spacing = group.spacing;
    padding = group.padding;
  }

  auto sum_main = 0.0f;
  auto max_cross = 0.0f;
  auto max_width = 0.0f;
  auto max_height = 0.0f;
  auto count = std::int32_t{0};

  for (const auto child_entity : node.get_component<scenes::relationship>().children) {
    auto child = scene.node_of(child_entity);

    if (!is_layout_child(child)) {
      continue;
    }

    ++count;

    const auto width = child_axis_size(scene, child, true);
    const auto height = child_axis_size(scene, child, false);

    if (is_horizontal_group) {
      sum_main += use_min ? width.min : width.preferred;
      max_cross = std::max(max_cross, use_min ? height.min : height.preferred);
    } else if (is_vertical_group) {
      sum_main += use_min ? height.min : height.preferred;
      max_cross = std::max(max_cross, use_min ? width.min : width.preferred);
    } else {
      max_width = std::max(max_width, use_min ? width.min : width.preferred);
      max_height = std::max(max_height, use_min ? height.min : height.preferred);
    }
  }

  if (is_horizontal_group) {
    const auto width = sum_main + spacing * static_cast<std::float_t>(std::max(0, count - 1)) + padding.x() + padding.z();
    const auto height = max_cross + padding.y() + padding.w();

    return math::vector2{width, height};
  }

  if (is_vertical_group) {
    const auto height = sum_main + spacing * static_cast<std::float_t>(std::max(0, count - 1)) + padding.y() + padding.w();
    const auto width = max_cross + padding.x() + padding.z();

    return math::vector2{width, height};
  }

  return math::vector2{max_width, max_height};
}

auto layout_resolver::horizontal_children(scenes::scene& scene, scenes::node node, const resolved_rect& rect) -> std::vector<std::pair<scenes::node, resolved_rect>> {
  auto result = std::vector<std::pair<scenes::node, resolved_rect>>{};

  if (!node.has_component<horizontal_layout_group>() || !node.has_component<scenes::relationship>()) {
    return result;
  }

  const auto& group = node.get_component<horizontal_layout_group>();

  _children.clear();

  for (const auto child_entity : node.get_component<scenes::relationship>().children) {
    auto child = scene.node_of(child_entity);

    if (is_layout_child(child)) {
      _children.push_back(child);
    }
  }

  if (_children.empty()) {
    return result;
  }

  const auto available_width = std::max(0.0f, rect.size.x() - group.padding.x() - group.padding.z() - group.spacing * static_cast<std::float_t>(_children.size() - 1));
  const auto available_height = std::max(0.0f, rect.size.y() - group.padding.y() - group.padding.w());

  _widths.assign(_children.size(), 0.0f);
  _heights.assign(_children.size(), 0.0f);
  _flexibles.assign(_children.size(), 0.0f);

  auto sum_preferred = 0.0f;
  auto total_flexible = 0.0f;

  for (auto index = std::size_t{0u}; index < _children.size(); ++index) {
    const auto width = child_axis_size(scene, _children[index], true);
    const auto height = child_axis_size(scene, _children[index], false);

    _widths[index] = width.preferred;
    _heights[index] = height.preferred;
    _flexibles[index] = width.flexible >= 0.0f && width.flexible > 0.0f ? width.flexible : (group.child_force_expand_width ? 1.0f : 0.0f);

    sum_preferred += width.preferred;
    total_flexible += _flexibles[index];
  }

  const auto extra = std::max(0.0f, available_width - sum_preferred);

  auto total_used_width = 0.0f;

  for (auto index = std::size_t{0u}; index < _children.size(); ++index) {
    if (group.control_child_width) {
      const auto share = total_flexible > 0.0f ? extra * (_flexibles[index] / total_flexible) : 0.0f;
      _widths[index] += share;
    }

    total_used_width += _widths[index];
  }

  total_used_width += group.spacing * static_cast<std::float_t>(_children.size() - 1);

  const auto horizontal_component = alignment_component(group.child_alignment, true);
  const auto vertical_component = alignment_component(group.child_alignment, false);

  auto cursor = rect.position.x() + group.padding.x() + alignment_offset(total_used_width, rect.size.x() - group.padding.x() - group.padding.z(), horizontal_component);

  for (auto index = std::size_t{0u}; index < _children.size(); ++index) {
    const auto height = group.control_child_height ? (group.child_force_expand_height ? available_height : std::clamp(_heights[index], 0.0f, available_height)) : _heights[index];
    const auto y = rect.position.y() + group.padding.y() + alignment_offset(height, available_height, vertical_component);

    result.emplace_back(_children[index], resolved_rect{math::vector2{cursor, y}, math::vector2{_widths[index], height}});

    cursor += _widths[index] + group.spacing;
  }

  return result;
}

auto layout_resolver::vertical_children(scenes::scene& scene, scenes::node node, const resolved_rect& rect) -> std::vector<std::pair<scenes::node, resolved_rect>> {
  auto result = std::vector<std::pair<scenes::node, resolved_rect>>{};

  if (!node.has_component<vertical_layout_group>() || !node.has_component<scenes::relationship>()) {
    return result;
  }

  const auto& group = node.get_component<vertical_layout_group>();

  _children.clear();

  for (const auto child_entity : node.get_component<scenes::relationship>().children) {
    auto child = scene.node_of(child_entity);

    if (is_layout_child(child)) {
      _children.push_back(child);
    }
  }

  if (_children.empty()) {
    return result;
  }

  const auto available_height = std::max(0.0f, rect.size.y() - group.padding.y() - group.padding.w() - group.spacing * static_cast<std::float_t>(_children.size() - 1));
  const auto available_width = std::max(0.0f, rect.size.x() - group.padding.x() - group.padding.z());

  _widths.assign(_children.size(), 0.0f);
  _heights.assign(_children.size(), 0.0f);
  _flexibles.assign(_children.size(), 0.0f);

  auto sum_preferred = 0.0f;
  auto total_flexible = 0.0f;

  for (auto index = std::size_t{0u}; index < _children.size(); ++index) {
    const auto width = child_axis_size(scene, _children[index], true);
    const auto height = child_axis_size(scene, _children[index], false);

    _widths[index] = width.preferred;
    _heights[index] = height.preferred;
    _flexibles[index] = height.flexible >= 0.0f && height.flexible > 0.0f ? height.flexible : (group.child_force_expand_height ? 1.0f : 0.0f);

    sum_preferred += height.preferred;
    total_flexible += _flexibles[index];
  }

  const auto extra = std::max(0.0f, available_height - sum_preferred);

  auto total_used_height = 0.0f;

  for (auto index = std::size_t{0u}; index < _children.size(); ++index) {
    if (group.control_child_height) {
      const auto share = total_flexible > 0.0f ? extra * (_flexibles[index] / total_flexible) : 0.0f;
      _heights[index] += share;
    }

    total_used_height += _heights[index];
  }

  total_used_height += group.spacing * static_cast<std::float_t>(_children.size() - 1);

  const auto horizontal_component = alignment_component(group.child_alignment, true);
  const auto vertical_component = alignment_component(group.child_alignment, false);

  auto cursor = rect.position.y() + group.padding.y() + alignment_offset(total_used_height, rect.size.y() - group.padding.y() - group.padding.w(), vertical_component);

  for (auto index = std::size_t{0u}; index < _children.size(); ++index) {
    const auto width = group.control_child_width ? (group.child_force_expand_width ? available_width : std::clamp(_widths[index], 0.0f, available_width)) : _widths[index];
    const auto x = rect.position.x() + group.padding.x() + alignment_offset(width, available_width, horizontal_component);

    result.emplace_back(_children[index], resolved_rect{math::vector2{x, cursor}, math::vector2{width, _heights[index]}});

    cursor += _heights[index] + group.spacing;
  }

  return result;
}

auto layout_resolver::grid_children(scenes::scene& scene, scenes::node node, const resolved_rect& rect) -> std::vector<std::pair<scenes::node, resolved_rect>> {
  auto result = std::vector<std::pair<scenes::node, resolved_rect>>{};

  if (!node.has_component<grid_layout_group>() || !node.has_component<scenes::relationship>()) {
    return result;
  }

  const auto& group = node.get_component<grid_layout_group>();

  _children.clear();

  for (const auto child_entity : node.get_component<scenes::relationship>().children) {
    auto child = scene.node_of(child_entity);

    if (is_layout_child(child)) {
      _children.push_back(child);
    }
  }

  if (_children.empty()) {
    return result;
  }

  const auto count = static_cast<std::int32_t>(_children.size());
  const auto available_width = std::max(0.0f, rect.size.x() - group.padding.x() - group.padding.z());
  const auto available_height = std::max(0.0f, rect.size.y() - group.padding.y() - group.padding.w());

  auto columns = std::int32_t{1};
  auto rows = std::int32_t{1};

  if (group.constraint == grid_constraint::fixed_column_count) {
    columns = std::max(1, group.constraint_count);
    rows = (count + columns - 1) / columns;
  } else if (group.constraint == grid_constraint::fixed_row_count) {
    rows = std::max(1, group.constraint_count);
    columns = (count + rows - 1) / rows;
  } else {
    columns = std::max(1, static_cast<std::int32_t>((available_width + group.spacing.x()) / (group.cell_size.x() + group.spacing.x())));
    rows = (count + columns - 1) / columns;
  }

  const auto content_width = static_cast<std::float_t>(columns) * group.cell_size.x() + static_cast<std::float_t>(columns - 1) * group.spacing.x();
  const auto content_height = static_cast<std::float_t>(rows) * group.cell_size.y() + static_cast<std::float_t>(rows - 1) * group.spacing.y();

  const auto horizontal_component = alignment_component(group.child_alignment, true);
  const auto vertical_component = alignment_component(group.child_alignment, false);

  const auto origin_x = rect.position.x() + group.padding.x() + alignment_offset(content_width, available_width, horizontal_component);
  const auto origin_y = rect.position.y() + group.padding.y() + alignment_offset(content_height, available_height, vertical_component);

  for (auto index = std::int32_t{0}; index < count; ++index) {
    auto col = std::int32_t{0};
    auto row = std::int32_t{0};

    if (group.start_axis == grid_start_axis::horizontal) {
      row = index / columns;
      col = index % columns;
    } else {
      col = index / rows;
      row = index % rows;
    }

    if (group.start_corner == grid_start_corner::upper_right || group.start_corner == grid_start_corner::lower_right) {
      col = columns - 1 - col;
    }

    if (group.start_corner == grid_start_corner::lower_left || group.start_corner == grid_start_corner::lower_right) {
      row = rows - 1 - row;
    }

    const auto x = origin_x + static_cast<std::float_t>(col) * (group.cell_size.x() + group.spacing.x());
    const auto y = origin_y + static_cast<std::float_t>(row) * (group.cell_size.y() + group.spacing.y());

    result.emplace_back(_children[static_cast<std::size_t>(index)], resolved_rect{math::vector2{x, y}, group.cell_size});
  }

  return result;
}

} // namespace sbx::canvas
