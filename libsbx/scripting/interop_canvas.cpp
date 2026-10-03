// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/interop.hpp>

#include <cstring>
#include <filesystem>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector4.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/canvas/canvas_module.hpp>
#include <libsbx/canvas/components.hpp>

#include <libsbx/scenes/components.hpp>

namespace sbx::scripting {

auto resolve_node(std::uint64_t uuid) -> scenes::node {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  return scene.find(math::uuid::from_value(uuid));
}

auto interop::canvas_get_sort_order(std::uint64_t uuid, std::int32_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->sort_order;
}

auto interop::canvas_set_sort_order(std::uint64_t uuid, std::int32_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->sort_order = value;
}

auto interop::rect_transform_get_anchor_min(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->anchor_min;
}

auto interop::rect_transform_set_anchor_min(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->anchor_min = *value;
}

auto interop::rect_transform_get_anchor_max(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->anchor_max;
}

auto interop::rect_transform_set_anchor_max(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->anchor_max = *value;
}

auto interop::rect_transform_get_anchored_position(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->anchored_position;
}

auto interop::rect_transform_set_anchored_position(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->anchored_position = *value;
}

auto interop::rect_transform_get_size_delta(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->size_delta;
}

auto interop::rect_transform_set_size_delta(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->size_delta = *value;
}

auto interop::rect_transform_get_pivot(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->pivot;
}

auto interop::rect_transform_set_pivot(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::rect_transform>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->pivot = *value;
}

auto interop::ui_image_get_tint(std::uint64_t uuid, math::color* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_image>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->tint;
}

auto interop::ui_image_set_tint(std::uint64_t uuid, math::color* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_image>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->tint = *value;
}

auto interop::ui_image_load_sprite(std::uint64_t uuid, managed::string path) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_image>();

  if (!node.is_valid() || !component) {
    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  component->sprite = assets_module.load_texture(std::filesystem::path{std::string{path}});
}

auto interop::ui_text_get_text(std::uint64_t uuid) -> managed::string {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!node.is_valid() || !component) {
    return managed::string::create("");
  }

  return managed::string::create(component->text);
}

auto interop::ui_text_set_text(std::uint64_t uuid, managed::string value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->text = std::string{value};
}

auto interop::ui_text_get_font_size(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->font_size;
}

auto interop::ui_text_set_font_size(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->font_size = value;
}

auto interop::ui_text_load_font(std::uint64_t uuid, managed::string path) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!node.is_valid() || !component) {
    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  component->font = assets_module.load_font(std::filesystem::path{std::string{path}});
}

auto interop::ui_text_get_color(std::uint64_t uuid, math::color* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->color;
}

auto interop::ui_text_set_color(std::uint64_t uuid, math::color* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->color = *value;
}

auto interop::ui_button_get_interactable(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  return node.is_valid() && component && component->interactable;
}

auto interop::ui_button_set_interactable(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->interactable = value;
}

auto interop::ui_button_get_normal_color(std::uint64_t uuid, math::color* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->normal_color;
}

auto interop::ui_button_set_normal_color(std::uint64_t uuid, math::color* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->normal_color = *value;
}

auto interop::ui_button_get_hovered_color(std::uint64_t uuid, math::color* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->hovered_color;
}

auto interop::ui_button_set_hovered_color(std::uint64_t uuid, math::color* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->hovered_color = *value;
}

auto interop::ui_button_get_pressed_color(std::uint64_t uuid, math::color* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->pressed_color;
}

auto interop::ui_button_set_pressed_color(std::uint64_t uuid, math::color* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->pressed_color = *value;
}

auto interop::ui_button_get_is_hovered(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  return node.is_valid() && component && component->is_hovered;
}

auto interop::ui_button_get_is_pressed(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  return node.is_valid() && component && component->is_pressed;
}

auto interop::ui_button_get_was_clicked(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_button>();

  return node.is_valid() && component && component->was_clicked;
}

auto interop::canvas_group_get_alpha(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->alpha;
}

auto interop::canvas_group_set_alpha(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->alpha = value;
}

auto interop::canvas_group_get_interactable(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  return node.is_valid() && component && component->interactable;
}

auto interop::canvas_group_set_interactable(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->interactable = value;
}

auto interop::canvas_group_get_blocks_raycasts(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  return node.is_valid() && component && component->blocks_raycasts;
}

auto interop::canvas_group_set_blocks_raycasts(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->blocks_raycasts = value;
}

auto interop::canvas_group_get_ignore_parent_groups(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  return node.is_valid() && component && component->ignore_parent_groups;
}

auto interop::canvas_group_set_ignore_parent_groups(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::canvas_group>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->ignore_parent_groups = value;
}

auto interop::canvas_wants_pointer_capture() -> managed::bool32 {
  auto& canvas_module = core::engine::get_module<canvas::canvas_module>();

  return canvas_module.wants_pointer_capture();
}

auto interop::ui_toggle_get_is_on(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_toggle>();

  return node.is_valid() && component && component->is_on;
}

auto interop::ui_toggle_set_is_on(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_toggle>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->is_on = value;
}

auto interop::ui_toggle_get_interactable(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_toggle>();

  return node.is_valid() && component && component->interactable;
}

auto interop::ui_toggle_set_interactable(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_toggle>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->interactable = value;
}

auto interop::ui_slider_get_value(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->value;
}

auto interop::ui_slider_set_value(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->value = value;
}

auto interop::ui_slider_get_min_value(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->min_value;
}

auto interop::ui_slider_set_min_value(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->min_value = value;
}

auto interop::ui_slider_get_max_value(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->max_value;
}

auto interop::ui_slider_set_max_value(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->max_value = value;
}

auto interop::ui_slider_get_whole_numbers(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  return node.is_valid() && component && component->whole_numbers;
}

auto interop::ui_slider_set_whole_numbers(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->whole_numbers = value;
}

auto interop::ui_slider_get_interactable(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  return node.is_valid() && component && component->interactable;
}

auto interop::ui_slider_set_interactable(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_slider>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->interactable = value;
}

auto interop::ui_scrollbar_get_value(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->value;
}

auto interop::ui_scrollbar_set_value(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->value = value;
}

auto interop::ui_scrollbar_get_size(std::uint64_t uuid, std::float_t* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->size;
}

auto interop::ui_scrollbar_set_size(std::uint64_t uuid, std::float_t value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->size = value;
}

auto interop::ui_scrollbar_get_interactable(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  return node.is_valid() && component && component->interactable;
}

auto interop::ui_scrollbar_set_interactable(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scrollbar>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->interactable = value;
}

auto interop::ui_scroll_rect_get_normalized_position(std::uint64_t uuid, math::vector2* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = component->normalized_position;
}

auto interop::ui_scroll_rect_set_normalized_position(std::uint64_t uuid, math::vector2* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->normalized_position = *value;
}

auto interop::ui_scroll_rect_get_horizontal(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  return node.is_valid() && component && component->horizontal;
}

auto interop::ui_scroll_rect_set_horizontal(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->horizontal = value;
}

auto interop::ui_scroll_rect_get_vertical(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  return node.is_valid() && component && component->vertical;
}

auto interop::ui_scroll_rect_set_vertical(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->vertical = value;
}

auto interop::ui_scroll_rect_set_content(std::uint64_t uuid, std::uint64_t content) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_scroll_rect>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->content = math::uuid::from_value(content);
}

auto interop::ui_text_get_alignment(std::uint64_t uuid, std::uint32_t* out_horizontal, std::uint32_t* out_vertical) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!out_horizontal || !out_vertical || !node.is_valid() || !component) {
    return;
  }

  *out_horizontal = static_cast<std::uint32_t>(component->horizontal_align);
  *out_vertical = static_cast<std::uint32_t>(component->vertical_align);
}

auto interop::ui_text_set_alignment(std::uint64_t uuid, std::uint32_t horizontal, std::uint32_t vertical) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_text>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->horizontal_align = static_cast<canvas::text_align>(horizontal);
  component->vertical_align = static_cast<canvas::text_align>(vertical);
}

auto interop::ui_mask_get_show_mask_graphic(std::uint64_t uuid) -> managed::bool32 {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_mask>();

  return node.is_valid() && component && component->show_mask_graphic;
}

auto interop::ui_mask_set_show_mask_graphic(std::uint64_t uuid, bool value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::ui_mask>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->show_mask_graphic = value;
}

// horizontal_layout_group and vertical_layout_group share every field.
template<typename Group>
auto read_layout_group(const Group& group) -> layout_group_data {
  return layout_group_data{
    .spacing = group.spacing,
    .padding_left = group.padding.x(),
    .padding_top = group.padding.y(),
    .padding_right = group.padding.z(),
    .padding_bottom = group.padding.w(),
    .child_alignment = static_cast<std::uint32_t>(group.child_alignment),
    .control_child_width = group.control_child_width ? 1u : 0u,
    .control_child_height = group.control_child_height ? 1u : 0u,
    .child_force_expand_width = group.child_force_expand_width ? 1u : 0u,
    .child_force_expand_height = group.child_force_expand_height ? 1u : 0u
  };
}

template<typename Group>
auto write_layout_group(Group& group, const layout_group_data& value) -> void {
  group.spacing = value.spacing;
  group.padding = math::vector4{value.padding_left, value.padding_top, value.padding_right, value.padding_bottom};
  group.child_alignment = static_cast<canvas::layout_alignment>(value.child_alignment);
  group.control_child_width = value.control_child_width != 0u;
  group.control_child_height = value.control_child_height != 0u;
  group.child_force_expand_width = value.child_force_expand_width != 0u;
  group.child_force_expand_height = value.child_force_expand_height != 0u;
}

auto interop::layout_group_get(std::uint64_t uuid, bool vertical, layout_group_data* out_value) -> void {
  auto node = resolve_node(uuid);

  if (!out_value || !node.is_valid()) {
    return;
  }

  if (vertical) {
    if (auto component = node.try_get_component<canvas::vertical_layout_group>()) {
      *out_value = read_layout_group(*component);
    }
  } else if (auto component = node.try_get_component<canvas::horizontal_layout_group>()) {
    *out_value = read_layout_group(*component);
  }
}

auto interop::layout_group_set(std::uint64_t uuid, bool vertical, const layout_group_data* value) -> void {
  auto node = resolve_node(uuid);

  if (!value || !node.is_valid()) {
    return;
  }

  if (vertical) {
    if (auto component = node.try_get_component<canvas::vertical_layout_group>()) {
      write_layout_group(*component, *value);
    }
  } else if (auto component = node.try_get_component<canvas::horizontal_layout_group>()) {
    write_layout_group(*component, *value);
  }
}

auto interop::layout_element_get(std::uint64_t uuid, layout_element_data* out_value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::layout_element>();

  if (!out_value || !node.is_valid() || !component) {
    return;
  }

  *out_value = layout_element_data{
    .min_width = component->min_width,
    .min_height = component->min_height,
    .preferred_width = component->preferred_width,
    .preferred_height = component->preferred_height,
    .flexible_width = component->flexible_width,
    .flexible_height = component->flexible_height,
    .ignore_layout = component->ignore_layout ? 1u : 0u
  };
}

auto interop::layout_element_set(std::uint64_t uuid, const layout_element_data* value) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::layout_element>();

  if (!value || !node.is_valid() || !component) {
    return;
  }

  component->min_width = value->min_width;
  component->min_height = value->min_height;
  component->preferred_width = value->preferred_width;
  component->preferred_height = value->preferred_height;
  component->flexible_width = value->flexible_width;
  component->flexible_height = value->flexible_height;
  component->ignore_layout = value->ignore_layout != 0u;
}

auto interop::content_size_fitter_get(std::uint64_t uuid, std::uint32_t* out_horizontal, std::uint32_t* out_vertical) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::content_size_fitter>();

  if (!out_horizontal || !out_vertical || !node.is_valid() || !component) {
    return;
  }

  *out_horizontal = static_cast<std::uint32_t>(component->horizontal_fit);
  *out_vertical = static_cast<std::uint32_t>(component->vertical_fit);
}

auto interop::content_size_fitter_set(std::uint64_t uuid, std::uint32_t horizontal, std::uint32_t vertical) -> void {
  auto node = resolve_node(uuid);
  auto component = node.try_get_component<canvas::content_size_fitter>();

  if (!node.is_valid() || !component) {
    return;
  }

  component->horizontal_fit = static_cast<canvas::content_fit_mode>(horizontal);
  component->vertical_fit = static_cast<canvas::content_fit_mode>(vertical);
}

} // namespace sbx::scripting
