// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/canvas/canvas_module.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/platform/input.hpp>
#include <libsbx/platform/mouse_button.hpp>
#include <libsbx/platform/window.hpp>

#include <libsbx/math/ray.hpp>
#include <libsbx/math/angle.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/constants.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

namespace sbx::canvas {

auto canvas_module::update() -> void {
  SBX_PROFILE_SCOPE("canvas_module::update");

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();

  // See _text_shape_cache's doc comment: a scene swap (e.g. exiting play mode) means every cached
  // uuid names a node in a registry that may no longer even exist, so drop them all rather than
  // look any of them up. Within the same scene, a ui_text node can still be individually destroyed
  // without a swap -- prune those too, each frame, via scene::find (canvas never resolves a uuid to
  // an entity itself). A uuid is a plain value either way, so this never risks touching live ECS
  // state for an id that no longer resolves to anything.
  if (&scene != _last_scene) {
    _text_shape_cache.clear();
    _last_scene = &scene;
  } else {
    for (auto it = _text_shape_cache.begin(); it != _text_shape_cache.end();) {
      if (!scene.find(it->first).is_valid()) {
        it = _text_shape_cache.erase(it);
      } else {
        ++it;
      }
    }
  }

  const auto target_extent = scene_renderer_module.target_extent();
  const auto screen_size = math::vector2{static_cast<std::float_t>(target_extent.x()), static_cast<std::float_t>(target_extent.y())};
  const auto mouse_position = platform::input::mouse_position() - scene_renderer_module.viewport_offset();
  const auto white_texture_index = assets_module.white_texture()->index();

  _draw_list.clear();
  _wants_pointer_capture = false;

  for (auto&& [entity, button] : scene.query<ui_button>().each()) {
    button.was_clicked = false;
  }

  auto canvases = std::vector<std::pair<scenes::node, canvas>>{};

  for (auto&& [entity, root] : scene.query<canvas>(ecs::exclude<scenes::inactive>).each()) {
    canvases.emplace_back(scene.node_of(entity), root);
  }

  std::sort(canvases.begin(), canvases.end(), [](const auto& lhs, const auto& rhs) { return lhs.second.sort_order < rhs.second.sort_order; });

  for (const auto& [node, root] : canvases) {
    if (root.mode != render_mode::screen_space_overlay) {
      continue;
    }

    if (!node.has_component<scenes::relationship>()) {
      continue;
    }

    auto scale_factor = 1.0f;
    auto virtual_size = screen_size;

    if (node.has_component<canvas_scaler>()) {
      const auto& scaler = node.get_component<canvas_scaler>();

      if (scaler.mode == canvas_scale_mode::scale_with_screen_size && scaler.reference_resolution.x() > 0.0f && scaler.reference_resolution.y() > 0.0f) {
        const auto log_width = std::log2(screen_size.x() / scaler.reference_resolution.x());
        const auto log_height = std::log2(screen_size.y() / scaler.reference_resolution.y());
        const auto blended = log_width + (log_height - log_width) * scaler.match_width_or_height;

        scale_factor = std::pow(2.0f, blended);
        virtual_size = screen_size / scale_factor;
      }
    }

    const auto root_rect = resolved_rect{math::vector2{0.0f, 0.0f}, virtual_size};
    const auto virtual_mouse_position = mouse_position / scale_factor;
    const auto root_state = canvas_inherited_state{1.0f, true, true, math::vector4{0.0f, 0.0f, screen_size.x(), screen_size.y()}};

    for (const auto child_entity : node.get_component<scenes::relationship>().children) {
      _visit(scene, scene.node_of(child_entity), root_rect, root_state, screen_size, virtual_mouse_position, white_texture_index, scale_factor, nullptr, nullptr, true);
    }
  }

  for (auto& [node, root] : canvases) {
    if (root.mode == render_mode::screen_space_overlay) {
      continue;
    }

    if (!node.has_component<scenes::relationship>()) {
      continue;
    }

    auto scale_factor = 1.0f;
    auto virtual_size = screen_size;

    if (node.has_component<canvas_scaler>()) {
      const auto& scaler = node.get_component<canvas_scaler>();

      if (scaler.mode == canvas_scale_mode::scale_with_screen_size && scaler.reference_resolution.x() > 0.0f && scaler.reference_resolution.y() > 0.0f) {
        const auto log_width = std::log2(screen_size.x() / scaler.reference_resolution.x());
        const auto log_height = std::log2(screen_size.y() / scaler.reference_resolution.y());
        const auto blended = log_width + (log_height - log_width) * scaler.match_width_or_height;

        scale_factor = std::pow(2.0f, blended);
        virtual_size = screen_size / scale_factor;
      }
    }

    const auto aspect = screen_size.y() > 0.0f ? screen_size.x() / screen_size.y() : 1.0f;

    auto view = math::matrix4x4::identity;
    auto projection = math::matrix4x4::identity;
    auto camera_world_matrix = math::matrix4x4::identity;
    auto camera_fov_degrees = 60.0f;
    auto camera_position = math::vector3{0.0f, 0.0f, 0.0f};

    if (root.mode == render_mode::screen_space_camera) {
      // Pinned to an explicit camera reference (Unity's Canvas.worldCamera equivalent) --
      // deliberately NOT edit-mode-aware, unlike world_space below: a canvas authored to render
      // through a specific camera should keep doing that regardless of which camera the editor
      // itself happens to be previewing through right now.
      auto camera_node = scene.find(root.camera);

      if (!camera_node.is_valid() || !camera_node.has_component<scenes::camera>()) {
        continue;
      }

      const auto& camera_settings = camera_node.get_component<scenes::camera>();

      camera_world_matrix = camera_node.world_matrix();
      camera_fov_degrees = camera_settings.fov_degrees;
      view = math::matrix4x4::inverted(camera_world_matrix);
      projection = math::matrix4x4::perspective(math::degree{camera_settings.fov_degrees}, aspect, camera_settings.near_plane, camera_settings.far_plane);
    } else {
      // world_space renders as part of the normal 3D view, so it must track whichever camera is
      // *actually* rendering this frame -- the editor's own fly-camera while play_state is "edit",
      // the scene's own active camera otherwise -- not just scene.active_camera() unconditionally,
      // or a world-space canvas would only ever line up correctly once you press Play.
      const auto camera = scene_renderer_module.effective_camera();

      if (!camera.is_active) {
        continue;
      }

      camera_fov_degrees = camera.fov_degrees;
      camera_position = camera.position;
      view = camera.view;
      projection = math::matrix4x4::perspective(math::degree{camera.fov_degrees}, aspect, camera.near_plane, camera.far_plane);
    }

    const auto view_projection = projection * view;

    auto canvas_to_world = math::matrix4x4::identity;
    auto mask_clip_supported = true;

    const auto make_pixel_to_local = [&virtual_size](std::float_t units_per_pixel) -> math::matrix4x4 {
      const auto translation = math::matrix4x4::translated(math::matrix4x4::identity, math::vector3{-virtual_size.x() * 0.5f * units_per_pixel, virtual_size.y() * 0.5f * units_per_pixel, 0.0f});
      const auto scale = math::matrix4x4::scaled(math::matrix4x4::identity, math::vector3{units_per_pixel, -units_per_pixel, 1.0f});

      return translation * scale;
    };

    if (root.mode == render_mode::screen_space_camera) {
      const auto plane_placement = camera_world_matrix * math::matrix4x4::translated(math::matrix4x4::identity, math::vector3{0.0f, 0.0f, -root.plane_distance});
      const auto half_height = root.plane_distance * math::tan(math::angle{math::degree{camera_fov_degrees * 0.5f}});
      const auto units_per_pixel = virtual_size.y() > 0.0f ? (2.0f * half_height) / virtual_size.y() : 1.0f;

      canvas_to_world = plane_placement * make_pixel_to_local(units_per_pixel);
    } else {
      auto placement = node.world_matrix();

      if (root.billboard) {
        const auto position = math::vector3{placement[3]};
        const auto to_camera = position - camera_position;

        // Same "+Z is the legible side" convention push_quad_world's winding relies on (see its
        // comment): quaternion::look_at aims local -Z (forward) at its direction argument, so
        // facing the viewer means aiming forward *away* from the camera, not at it.
        if (to_camera.length_squared() > math::epsilonf) {
          const auto rotation = math::quaternion::look_at(math::vector3::normalized(to_camera));

          placement = math::matrix4x4::translated(math::matrix4x4::identity, position) * math::matrix_cast<math::matrix4x4>(rotation);
        }
      }

      canvas_to_world = placement * make_pixel_to_local(root.world_scale);
      mask_clip_supported = false;
    }

    const auto world_mvp = view_projection * canvas_to_world;

    auto virtual_mouse_position = math::vector2{-1e9f, -1e9f};

    {
      const auto inverse_view_projection = math::matrix4x4::inverted(view_projection);

      const auto ndc_x = screen_size.x() > 0.0f ? (mouse_position.x() / screen_size.x()) * 2.0f - 1.0f : 0.0f;
      const auto ndc_y = screen_size.y() > 0.0f ? (mouse_position.y() / screen_size.y()) * 2.0f - 1.0f : 0.0f;

      const auto unproject = [&inverse_view_projection, ndc_x, ndc_y](std::float_t ndc_z) -> math::vector3 {
        const auto point = inverse_view_projection * math::vector4{ndc_x, ndc_y, ndc_z, 1.0f};
        return math::vector3{point.x(), point.y(), point.z()} / point.w();
      };

      const auto near_point = unproject(0.0f);
      const auto far_point = unproject(1.0f);
      const auto ray = math::ray{near_point, far_point - near_point};

      const auto plane_point = math::vector3{canvas_to_world[3]};
      const auto plane_normal = math::vector3::normalized(math::vector3{canvas_to_world[2]});

      const auto denom = math::vector3::dot(ray.direction(), plane_normal);

      if (std::abs(denom) > 1e-6f) {
        const auto t = math::vector3::dot(plane_point - ray.origin(), plane_normal) / denom;

        if (t >= 0.0f) {
          const auto world_hit = ray.point_at(t);
          const auto local = math::matrix4x4::inverted(canvas_to_world) * math::vector4{world_hit, 1.0f};

          virtual_mouse_position = math::vector2{local.x(), local.y()};
        }
      }
    }

    const auto root_rect = resolved_rect{math::vector2{0.0f, 0.0f}, virtual_size};
    const auto root_state = canvas_inherited_state{1.0f, true, true, math::vector4{0.0f, 0.0f, screen_size.x(), screen_size.y()}};

    for (const auto child_entity : node.get_component<scenes::relationship>().children) {
      _visit(scene, scene.node_of(child_entity), root_rect, root_state, screen_size, virtual_mouse_position, white_texture_index, scale_factor, nullptr, &world_mvp, mask_clip_supported);
    }
  }
}

auto canvas_module::_emit_quad(const math::vector2& position, const math::vector2& size, const math::vector4& uv_rect, std::uint32_t texture_index, const math::color& color, const math::vector2& screen_size, const math::vector4& clip_rect, const math::matrix4x4* world_mvp) -> void {
  if (world_mvp != nullptr) {
    _draw_list.add_quad_world(position, size, uv_rect, texture_index, color, *world_mvp, clip_rect);
  } else {
    _draw_list.add_quad(position, size, uv_rect, texture_index, color, screen_size, clip_rect);
  }
}

auto canvas_module::_emit_glyph_quad(const math::vector2& position, const math::vector2& size, const math::vector4& uv_rect, std::uint32_t texture_index, const math::color& color, const math::vector2& screen_size, const math::vector4& clip_rect, const math::matrix4x4* world_mvp) -> void {
  if (world_mvp != nullptr) {
    _draw_list.add_glyph_quad_world(position, size, uv_rect, texture_index, color, *world_mvp, clip_rect);
  } else {
    _draw_list.add_glyph_quad(position, size, uv_rect, texture_index, color, screen_size, clip_rect);
  }
}

auto canvas_module::_visit(scenes::scene& scene, scenes::node node, const resolved_rect& parent_rect, const canvas_inherited_state& inherited, const math::vector2& screen_size, const math::vector2& mouse_position, std::uint32_t white_texture_index, std::float_t scale_factor, const resolved_rect* rect_override, const math::matrix4x4* world_mvp, bool mask_clip_supported) -> void {
  auto rect_transform_component = node.try_get_component<rect_transform>();

  if (!rect_transform_component) {
    return;
  }

  // An inactive node draws nothing and blocks nothing -- and since children are only ever reached
  // by recursing further down in this same function, returning here skips its whole subtree too,
  // same as Unity's SetActive(false) on a parent.
  if (node.has_component<scenes::inactive>()) {
    return;
  }

  auto state = inherited;

  if (auto group = node.try_get_component<canvas_group>()) {
    if (group->ignore_parent_groups) {
      state = canvas_inherited_state{group->alpha, group->interactable, group->blocks_raycasts};
    } else {
      state = canvas_inherited_state{inherited.alpha * group->alpha, inherited.interactable && group->interactable, inherited.blocks_raycasts && group->blocks_raycasts};
    }
  }

  auto rect = resolved_rect{};

  if (rect_override != nullptr) {
    rect = *rect_override;
  } else {
    auto rt = *rect_transform_component;

    if (auto fitter = node.try_get_component<content_size_fitter>()) {
      if (fitter->horizontal_fit != content_fit_mode::unconstrained || fitter->vertical_fit != content_fit_mode::unconstrained) {
        if (fitter->horizontal_fit == content_fit_mode::preferred_size) {
          rt.size_delta.x() = layout_resolver::compute_preferred_size(scene, node, false).x();
        } else if (fitter->horizontal_fit == content_fit_mode::min_size) {
          rt.size_delta.x() = layout_resolver::compute_preferred_size(scene, node, true).x();
        }

        if (fitter->vertical_fit == content_fit_mode::preferred_size) {
          rt.size_delta.y() = layout_resolver::compute_preferred_size(scene, node, false).y();
        } else if (fitter->vertical_fit == content_fit_mode::min_size) {
          rt.size_delta.y() = layout_resolver::compute_preferred_size(scene, node, true).y();
        }
      }
    }

    rect = resolve_rect(rt, parent_rect);
  }

  const auto screen_rect = resolved_rect{rect.position * scale_factor, rect.size * scale_factor};

  auto mask = node.try_get_component<ui_mask>();

  if (mask && mask_clip_supported) {
    state.clip_rect = math::vector4{
      std::max(state.clip_rect.x(), screen_rect.position.x()),
      std::max(state.clip_rect.y(), screen_rect.position.y()),
      std::min(state.clip_rect.z(), screen_rect.position.x() + screen_rect.size.x()),
      std::min(state.clip_rect.w(), screen_rect.position.y() + screen_rect.size.y())
    };
  }

  const auto is_over =
    mouse_position.x() >= rect.position.x() && mouse_position.x() <= rect.position.x() + rect.size.x() &&
    mouse_position.y() >= rect.position.y() && mouse_position.y() <= rect.position.y() + rect.size.y();

  const auto full_uv_rect = math::vector4{0.0f, 0.0f, 1.0f, 1.0f};

  const auto draws_own_graphic = !mask || mask->show_mask_graphic;

  if (auto button_component = node.try_get_component<ui_button>()) {
    auto& button = *button_component;

    if (button.interactable && state.interactable && state.blocks_raycasts) {
      if (is_over) {
        _wants_pointer_capture = true;

        if (platform::input::is_mouse_button_pressed(platform::mouse_button::left)) {
          button.is_pressed = true;
        }
      }

      if (button.is_pressed && platform::input::is_mouse_button_released(platform::mouse_button::left)) {
        button.was_clicked = is_over;
        button.is_pressed = false;

        if (button.was_clicked) {
          _on_button_clicked.emit(node);
        }
      }

      button.is_hovered = is_over;
    }

    if (draws_own_graphic) {
      auto fill_color = button.is_pressed ? button.pressed_color : (button.is_hovered ? button.hovered_color : button.normal_color);
      fill_color.a() *= state.alpha;

      _emit_quad(screen_rect.position, screen_rect.size, full_uv_rect, white_texture_index, fill_color, screen_size, state.clip_rect, world_mvp);
    }
  } else if (auto image_component = node.try_get_component<ui_image>()) {
    const auto& image = *image_component;

    if (image.raycast_target && state.blocks_raycasts && is_over) {
      _wants_pointer_capture = true;
    }

    if (draws_own_graphic) {
      auto tint = image.tint;
      tint.a() *= state.alpha;

      if (image.sprite && image.sprite->is_valid()) {
        _emit_quad(screen_rect.position, screen_rect.size, image.uv_rect, image.sprite->index(), tint, screen_size, state.clip_rect, world_mvp);
      } else {
        _emit_quad(screen_rect.position, screen_rect.size, full_uv_rect, white_texture_index, tint, screen_size, state.clip_rect, world_mvp);
      }
    }
  } else if (auto text_component = node.try_get_component<ui_text>()) {
    const auto& text = *text_component;

    if (text.raycast_target && state.blocks_raycasts && is_over) {
      _wants_pointer_capture = true;
    }

    if (draws_own_graphic) {
      auto color = text.color;
      color.a() *= state.alpha;

      const auto key = text_shape_cache_key{text.text, text.font.get(), text.font ? text.font->generation() : 0u, text.font_size, text.horizontal_align, text.vertical_align, text.line_spacing, rect};
      auto& cache_entry = _text_shape_cache[node.id()];

      if (!(cache_entry.key == key)) {
        cache_entry.glyphs = shape_text(text, rect);
        cache_entry.key = key;
      }

      for (const auto& glyph : cache_entry.glyphs) {
        _emit_glyph_quad(glyph.rect.position * scale_factor, glyph.rect.size * scale_factor, glyph.uv_rect, glyph.texture_index, color, screen_size, state.clip_rect, world_mvp);
      }
    }
  } else if (auto toggle_component = node.try_get_component<ui_toggle>()) {
    auto& toggle = *toggle_component;

    if (toggle.interactable && state.interactable && state.blocks_raycasts) {
      if (is_over) {
        _wants_pointer_capture = true;

        if (platform::input::is_mouse_button_pressed(platform::mouse_button::left)) {
          toggle.is_pressed = true;
        }
      }

      if (toggle.is_pressed && platform::input::is_mouse_button_released(platform::mouse_button::left)) {
        toggle.is_pressed = false;

        if (is_over) {
          toggle.is_on = !toggle.is_on;

          if (toggle.is_on && toggle.group != math::uuid::nil()) {
            for (auto&& [other_entity, other_toggle] : scene.query<ui_toggle>().each()) {
              if (&other_toggle != &toggle && other_toggle.group == toggle.group) {
                other_toggle.is_on = false;
              }
            }
          }

          _on_value_changed.emit(node);
        }
      }
    }

    if (draws_own_graphic) {
      auto fill_color = toggle.is_on ? toggle.on_color : toggle.off_color;
      fill_color.a() *= state.alpha;

      _emit_quad(screen_rect.position, screen_rect.size, full_uv_rect, white_texture_index, fill_color, screen_size, state.clip_rect, world_mvp);
    }
  } else if (auto slider_component = node.try_get_component<ui_slider>()) {
    auto& slider = *slider_component;

    if (slider.interactable && state.interactable && state.blocks_raycasts) {
      if (is_over) {
        _wants_pointer_capture = true;

        if (platform::input::is_mouse_button_pressed(platform::mouse_button::left)) {
          slider.is_dragging = true;
        }
      }

      if (platform::input::is_mouse_button_released(platform::mouse_button::left)) {
        slider.is_dragging = false;
      }

      if (slider.is_dragging) {
        _wants_pointer_capture = true;

        const auto fraction = std::clamp(
          slider.direction == slider_direction::horizontal
            ? (mouse_position.x() - rect.position.x()) / rect.size.x()
            : (mouse_position.y() - rect.position.y()) / rect.size.y(),
          0.0f, 1.0f
        );

        auto new_value = slider.min_value + fraction * (slider.max_value - slider.min_value);

        if (slider.whole_numbers) {
          new_value = std::round(new_value);
        }

        if (new_value != slider.value) {
          slider.value = new_value;
          _on_value_changed.emit(node);
        }
      }
    }

    if (draws_own_graphic) {
      auto track_tint = slider.track_color;
      track_tint.a() *= state.alpha;

      _emit_quad(screen_rect.position, screen_rect.size, full_uv_rect, white_texture_index, track_tint, screen_size, state.clip_rect, world_mvp);

      const auto fill_fraction = slider.max_value > slider.min_value ? std::clamp((slider.value - slider.min_value) / (slider.max_value - slider.min_value), 0.0f, 1.0f) : 0.0f;

      auto fill_position = screen_rect.position;
      auto fill_size = screen_rect.size;

      if (slider.direction == slider_direction::horizontal) {
        fill_size.x() *= fill_fraction;
      } else {
        const auto filled_height = screen_rect.size.y() * fill_fraction;
        fill_position.y() = screen_rect.position.y() + (screen_rect.size.y() - filled_height);
        fill_size.y() = filled_height;
      }

      auto fill_tint = slider.fill_color;
      fill_tint.a() *= state.alpha;

      _emit_quad(fill_position, fill_size, full_uv_rect, white_texture_index, fill_tint, screen_size, state.clip_rect, world_mvp);
    }
  } else if (auto scrollbar_component = node.try_get_component<ui_scrollbar>()) {
    auto& scrollbar = *scrollbar_component;

    if (scrollbar.interactable && state.interactable && state.blocks_raycasts) {
      if (is_over) {
        _wants_pointer_capture = true;

        if (platform::input::is_mouse_button_pressed(platform::mouse_button::left)) {
          scrollbar.is_dragging = true;
        }
      }

      if (platform::input::is_mouse_button_released(platform::mouse_button::left)) {
        scrollbar.is_dragging = false;
      }

      if (scrollbar.is_dragging) {
        _wants_pointer_capture = true;

        const auto fraction = std::clamp(
          scrollbar.direction == slider_direction::horizontal
            ? (mouse_position.x() - rect.position.x()) / rect.size.x()
            : (mouse_position.y() - rect.position.y()) / rect.size.y(),
          0.0f, 1.0f
        );

        if (fraction != scrollbar.value) {
          scrollbar.value = fraction;
          _on_value_changed.emit(node);
        }
      }
    }

    if (draws_own_graphic) {
      auto track_tint = scrollbar.track_color;
      track_tint.a() *= state.alpha;

      _emit_quad(screen_rect.position, screen_rect.size, full_uv_rect, white_texture_index, track_tint, screen_size, state.clip_rect, world_mvp);

      const auto handle_fraction = std::clamp(scrollbar.size, 0.0f, 1.0f);

      auto handle_position = screen_rect.position;
      auto handle_size = screen_rect.size;

      if (scrollbar.direction == slider_direction::horizontal) {
        handle_size.x() = screen_rect.size.x() * handle_fraction;
        handle_position.x() = screen_rect.position.x() + scrollbar.value * (screen_rect.size.x() - handle_size.x());
      } else {
        handle_size.y() = screen_rect.size.y() * handle_fraction;
        handle_position.y() = screen_rect.position.y() + scrollbar.value * (screen_rect.size.y() - handle_size.y());
      }

      auto handle_tint = scrollbar.handle_color;
      handle_tint.a() *= state.alpha;

      _emit_quad(handle_position, handle_size, full_uv_rect, white_texture_index, handle_tint, screen_size, state.clip_rect, world_mvp);
    }
  }

  if (auto relationship_component = node.try_get_component<scenes::relationship>()) {
    auto group_layout = std::vector<std::pair<scenes::node, resolved_rect>>{};
    auto has_group = false;

    if (node.has_component<horizontal_layout_group>()) {
      group_layout = _layout_resolver.horizontal_children(scene, node, rect);
      has_group = true;
    } else if (node.has_component<vertical_layout_group>()) {
      group_layout = _layout_resolver.vertical_children(scene, node, rect);
      has_group = true;
    } else if (node.has_component<grid_layout_group>()) {
      group_layout = _layout_resolver.grid_children(scene, node, rect);
      has_group = true;
    }

    if (auto scroll_component = node.try_get_component<ui_scroll_rect>()) {
      auto& scroll = *scroll_component;

      for (const auto child_entity : relationship_component->children) {
        auto child = scene.node_of(child_entity);

        auto child_rect_transform = child.try_get_component<rect_transform>();

        if (!child_rect_transform || child.id().value() != scroll.content.value()) {
          continue;
        }

        const auto natural = resolve_rect(*child_rect_transform, rect);

        const auto max_scroll_x = std::max(0.0f, natural.size.x() - rect.size.x());
        const auto max_scroll_y = std::max(0.0f, natural.size.y() - rect.size.y());

        if (is_over) {
          const auto wheel = platform::input::scroll_delta();

          if (scroll.vertical && max_scroll_y > 0.0f) {
            scroll.normalized_position.y() = std::clamp(scroll.normalized_position.y() - wheel.y() * 0.05f, 0.0f, 1.0f);
          }

          if (scroll.horizontal && max_scroll_x > 0.0f) {
            scroll.normalized_position.x() = std::clamp(scroll.normalized_position.x() + wheel.x() * 0.05f, 0.0f, 1.0f);
          }

          if (platform::input::is_mouse_button_pressed(platform::mouse_button::left)) {
            scroll.is_dragging = true;
            scroll.drag_start_mouse = mouse_position;
            scroll.drag_start_normalized = scroll.normalized_position;
          }
        }

        if (platform::input::is_mouse_button_released(platform::mouse_button::left)) {
          scroll.is_dragging = false;
        }

        if (scroll.is_dragging) {
          _wants_pointer_capture = true;

          const auto delta = mouse_position - scroll.drag_start_mouse;

          if (scroll.horizontal && max_scroll_x > 0.0f) {
            scroll.normalized_position.x() = std::clamp(scroll.drag_start_normalized.x() - delta.x() / max_scroll_x, 0.0f, 1.0f);
          }

          if (scroll.vertical && max_scroll_y > 0.0f) {
            scroll.normalized_position.y() = std::clamp(scroll.drag_start_normalized.y() - delta.y() / max_scroll_y, 0.0f, 1.0f);
          }
        }

        const auto offset = math::vector2{-scroll.normalized_position.x() * max_scroll_x, -scroll.normalized_position.y() * max_scroll_y};

        group_layout.emplace_back(child, resolved_rect{natural.position + offset, natural.size});
        has_group = true;

        break;
      }
    }

    for (const auto child_entity : relationship_component->children) {
      auto child = scene.node_of(child_entity);

      if (has_group) {
        const auto entry = std::find_if(group_layout.begin(), group_layout.end(), [&](const auto& pair) { return pair.first == child; });

        if (entry != group_layout.end()) {
          _visit(scene, child, rect, state, screen_size, mouse_position, white_texture_index, scale_factor, &entry->second, world_mvp, mask_clip_supported);
          continue;
        }
      }

      _visit(scene, child, rect, state, screen_size, mouse_position, white_texture_index, scale_factor, nullptr, world_mvp, mask_clip_supported);
    }
  }
}

} // namespace sbx::canvas
