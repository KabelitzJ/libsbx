// SPDX-License-Identifier: MIT
#include <libsbx/sprites/sprites_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

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

auto _sprite_kind(const sprites::screen_overlay_sprite&) -> const char* {
  return "screen_overlay_sprite";
}

auto _sprite_kind(const sprites::screen_camera_sprite&) -> const char* {
  return "screen_camera_sprite";
}

auto _sprite_kind(const sprites::world_sprite&) -> const char* {
  return "world_sprite";
}

auto _emit_optional_image(YAML::Emitter& emitter, scenes::asset_registry& registry, const char* key, const graphics::image2d_handle& image) -> void {
  if (image == graphics::image2d_handle{}) {
    return;
  }

  const auto& metadata = registry.image_metadata(image);

  emitter << YAML::Key << key << YAML::Value << YAML::Alias(metadata.name);
}

auto _read_optional_image(const YAML::Node& node, const char* key, scenes::asset_registry& registry, graphics::image2d_handle& image) -> void {
  if (const auto image_node = node[key]; image_node) {
    const auto image_name = image_node["name"].as<std::string>();

    image = registry.get_image(utility::hashed_string{image_name});
  }
}

template<typename T>
auto _emit_if_not_default(YAML::Emitter& emitter, const char* key, const T& value, const T& default_value) -> void {
  if (value != default_value) {
    emitter << YAML::Key << key << YAML::Value << value;
  }
}

template<typename Sprite>
auto _emit_sprite_common(YAML::Emitter& emitter, scenes::asset_registry& registry, const Sprite& sprite) -> void {
  const auto defaults = Sprite{};

  _emit_if_not_default(emitter, "base_color", sprite.base_color, defaults.base_color);
  _emit_optional_image(emitter, registry, "albedo_image", sprite.albedo_image);
  _emit_if_not_default(emitter, "emissive_factor", sprite.emissive_factor, defaults.emissive_factor);
  _emit_if_not_default(emitter, "emissive_strength", sprite.emissive_strength, defaults.emissive_strength);
  _emit_optional_image(emitter, registry, "emissive_image", sprite.emissive_image);
  _emit_if_not_default(emitter, "size", sprite.size, defaults.size);
  _emit_if_not_default(emitter, "pivot", sprite.pivot, defaults.pivot);
}

template<typename Sprite>
auto _read_sprite_common(const YAML::Node& node, scenes::asset_registry& registry, Sprite& sprite) -> void {
  if (const auto value = node["base_color"]; value) {
    sprite.base_color = value.as<math::color>();
  }

  _read_optional_image(node, "albedo_image", registry, sprite.albedo_image);

  if (const auto value = node["emissive_factor"]; value) {
    sprite.emissive_factor = value.as<math::color>();
  }

  if (const auto value = node["emissive_strength"]; value) {
    sprite.emissive_strength = value.as<float>();
  }

  _read_optional_image(node, "emissive_image", registry, sprite.emissive_image);

  if (const auto value = node["size"]; value) {
    sprite.size = value.as<math::vector2>();
  }

  if (const auto value = node["pivot"]; value) {
    sprite.pivot = value.as<math::vector2>();
  }
}

auto _emit_sprite_extra(YAML::Emitter& emitter, const sprites::screen_overlay_sprite& sprite) -> void {
  const auto defaults = sprites::screen_overlay_sprite{};

  _emit_if_not_default(emitter, "position", sprite.position, defaults.position);
  _emit_if_not_default(emitter, "sort_order", sprite.sort_order, defaults.sort_order);
}

auto _emit_sprite_extra(YAML::Emitter& emitter, const sprites::screen_camera_sprite& sprite) -> void {
  const auto defaults = sprites::screen_camera_sprite{};

  _emit_if_not_default(emitter, "position", sprite.position, defaults.position);
  _emit_if_not_default(emitter, "depth", sprite.depth, defaults.depth);
  _emit_if_not_default(emitter, "sort_order", sprite.sort_order, defaults.sort_order);
}

auto _emit_sprite_extra(YAML::Emitter& emitter, const sprites::world_sprite& sprite) -> void {
  const auto defaults = sprites::world_sprite{};

  _emit_if_not_default(emitter, "is_billboard", sprite.is_billboard, defaults.is_billboard);
}

auto _read_sprite_extra(const YAML::Node& node, sprites::screen_overlay_sprite& sprite) -> void {
  if (const auto value = node["position"]; value) {
    sprite.position = value.as<math::vector2>();
  }

  if (const auto value = node["sort_order"]; value) {
    sprite.sort_order = value.as<std::int32_t>();
  }
}

auto _read_sprite_extra(const YAML::Node& node, sprites::screen_camera_sprite& sprite) -> void {
  if (const auto value = node["position"]; value) {
    sprite.position = value.as<math::vector2>();
  }

  if (const auto value = node["depth"]; value) {
    sprite.depth = value.as<float>();
  }

  if (const auto value = node["sort_order"]; value) {
    sprite.sort_order = value.as<std::int32_t>();
  }
}

auto _read_sprite_extra(const YAML::Node& node, sprites::world_sprite& sprite) -> void {
  if (const auto value = node["is_billboard"]; value) {
    sprite.is_billboard = value.as<bool>();
  }
}

sprites_module::sprites_module() {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& component_io_registry = scenes_module.get_component_io_registry();

  component_io_registry.register_component<sprites::sprite>(
    "sprite",
    [](YAML::Emitter& emitter, [[maybe_unused]] scenes::scene_graph& graph, scenes::asset_registry& registry, const sprites::sprite& sprite) -> void {
      std::visit(
        [&](const auto& value) -> void {
          emitter << YAML::Key << "kind" << YAML::Value << _sprite_kind(value);

          _emit_sprite_common(emitter, registry, value);
          _emit_sprite_extra(emitter, value);
        },
        sprite
      );
    },
    [](const YAML::Node& node, [[maybe_unused]] scenes::scene_graph& graph, scenes::asset_registry& registry) -> sprites::sprite {
      const auto kind = node["kind"].as<std::string>();

      if (kind == "screen_overlay_sprite") {
        auto sprite = sprites::screen_overlay_sprite{};

        _read_sprite_common(node, registry, sprite);
        _read_sprite_extra(node, sprite);

        return sprite;
      }

      if (kind == "screen_camera_sprite") {
        auto sprite = sprites::screen_camera_sprite{};

        _read_sprite_common(node, registry, sprite);
        _read_sprite_extra(node, sprite);

        return sprite;
      }

      if (kind == "world_sprite") {
        auto sprite = sprites::world_sprite{};

        _read_sprite_common(node, registry, sprite);
        _read_sprite_extra(node, sprite);

        return sprite;
      }

      throw utility::runtime_error{"Unknown sprite kind '{}'", kind};
    }
  );
}

auto sprites_module::update() -> void {
  EASY_BLOCK("sprites_module::update");

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
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto sprite_query = graph.query<const sprites::sprite>();

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

        const auto& transform = graph.world_transform(node);

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
