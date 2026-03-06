// SPDX-License-Identifier: MIT
#include <libsbx/sprites/sprites_module.hpp>

namespace sbx::sprites {

auto sprite_batch::clear() -> void {
  _instances.clear();
}

auto sprite_batch::add(const sprite_instance& instance) -> void {
  _instances.push_back(instance);
}

auto sprite_batch::sort() -> void {
  std::sort(_instances.begin(), _instances.end(), [](const sprite_instance& a, const sprite_instance& b) {
    return a.sort_order < b.sort_order;
  });
}

[[nodiscard]] auto sprite_batch::instances() const -> const std::vector<sprite_instance>& {
  return _instances;
}

[[nodiscard]] auto sprite_batch::size() const -> std::size_t {
  return _instances.size();
}

[[nodiscard]] auto sprite_batch::is_empty() const -> bool {
  return _instances.empty();
}

[[nodiscard]] auto sprite_batch::data() const -> const sprite_instance* {
  return _instances.data();
}

[[nodiscard]] auto sprite_batch::byte_size() const -> std::size_t {
  return _instances.size() * sizeof(sprite_instance);
}

auto sprites_module::update() -> void {
  for (auto& batch : _batches) {
    batch.clear();
  }

  _images.clear();

  _collect_sprites();
}

auto sprites_module::submit(sprite_space space, const sprite_batch::sprite_instance& instance) -> void {
  _batches[utility::to_underlying(space)].add(instance);
}

[[nodiscard]] auto sprites_module::register_image(graphics::image2d_handle image) -> std::uint32_t {
  return _images.push_back(image);
}

[[nodiscard]] auto sprites_module::register_image(const std::string& attachment) -> std::uint32_t {
  return _images.push_back(attachment);
}

[[nodiscard]] auto sprites_module::batch(sprite_space space) -> sprite_batch& {
  return _batches[utility::to_underlying(space)];
}

[[nodiscard]] auto sprites_module::batch(sprite_space space) const -> const sprite_batch& {
  return _batches[utility::to_underlying(space)];
}

[[nodiscard]] auto sprites_module::images() -> graphics::separate_image2d_array& {
  return _images;
}

[[nodiscard]] auto sprites_module::images() const -> const graphics::separate_image2d_array& {
  return _images;
}

auto sprites_module::_collect_sprites() -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  auto sprite_query = scene.query<const sprites::sprite>();

  for (auto&& [node, sprite] : sprite_query.each()) {
    std::visit(utility::overload{
      [&](const screen_overlay_sprite& sprite) {
        const auto albedo_index = _images.push_back(sprite.albedo_image);
        const auto emissive_index = _images.push_back(sprite.emissive_image);

        _batches[utility::to_underlying(sprite_space::screen_overlay)].add(sprite_batch::sprite_instance{
          .model = math::matrix4x4::translated(math::matrix4x4::identity, math::vector3{sprite.position.x(), sprite.position.y(), 0.0f}),
          .base_color = sprite.base_color,
          .emissive_factor = sprite.emissive_factor,
          .size = sprite.size,
          .pivot = sprite.pivot,
          .uv_min = {0.0f, 0.0f},
          .uv_max = {1.0f, 1.0f},
          .emissive_strength = sprite.emissive_strength,
          .albedo_image_index = albedo_index,
          .emissive_image_index = emissive_index,
          .sort_order = sprite.sort_order,
          .flags = 0u,
          .sdf_px_range = 0.0f,
          .padding0 = 0u,
          .padding1 = 0u
        });
      },
      [&](const screen_camera_sprite& sprite) {
        const auto albedo_index = _images.push_back(sprite.albedo_image);
        const auto emissive_index = _images.push_back(sprite.emissive_image);

        _batches[utility::to_underlying(sprite_space::screen_camera)].add(sprite_batch::sprite_instance{
          .model = math::matrix4x4::translated(math::matrix4x4::identity, math::vector3{sprite.position.x(), sprite.position.y(), sprite.depth}),
          .base_color = sprite.base_color,
          .emissive_factor = sprite.emissive_factor,
          .size = sprite.size,
          .pivot = sprite.pivot,
          .uv_min = {0.0f, 0.0f},
          .uv_max = {1.0f, 1.0f},
          .emissive_strength = sprite.emissive_strength,
          .albedo_image_index = albedo_index,
          .emissive_image_index = emissive_index,
          .sort_order = sprite.sort_order,
          .flags = 0u,
          .sdf_px_range = 0.0f,
          .padding0 = 0u,
          .padding1 = 0u
        });
      },
      [&](const world_sprite& sprite) {
        const auto albedo_index = _images.push_back(sprite.albedo_image);
        const auto emissive_index = _images.push_back(sprite.emissive_image);

        const auto& transform = scene.world_transform(node);

        auto flags = std::uint32_t{0u};

        if (sprite.is_billboard) {
          flags |= sprite_batch::flag_is_billboard;
        }

        _batches[utility::to_underlying(sprite_space::world)].add(sprite_batch::sprite_instance{
          .model = transform,
          .base_color = sprite.base_color,
          .emissive_factor = sprite.emissive_factor,
          .size = sprite.size,
          .pivot = sprite.pivot,
          .uv_min = {0.0f, 0.0f},
          .uv_max = {1.0f, 1.0f},
          .emissive_strength = sprite.emissive_strength,
          .albedo_image_index = albedo_index,
          .emissive_image_index = emissive_index,
          .sort_order = 0,
          .flags = flags,
          .sdf_px_range = 0.0f,
          .padding0 = 0u,
          .padding1 = 0u
        });
      }
    }, sprite);
  }
}

} // namespace sbx::sprites