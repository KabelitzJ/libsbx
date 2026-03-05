// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SPRITES_SPRITES_MODULE_HPP_
#define LIBSBX_SPRITES_SPRITES_MODULE_HPP_

#include <vector>
#include <variant>
#include <array>
#include <algorithm>

#include <magic_enum/magic_enum.hpp>

#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/overload.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/graphics/images/image.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>

#include <libsbx/scenes/scenes_module.hpp>

namespace sbx::sprites {

enum class sprite_space : std::uint32_t {
  screen_overlay = 0,
  screen_camera = 1,
  world = 2
}; // enum class sprite_space

struct screen_overlay_sprite {
  math::color base_color{1.0f, 1.0f, 1.0f, 1.0f};
  graphics::image2d_handle albedo_image{};
  math::color emissive_factor{0.0f, 0.0f, 0.0f, 1.0f};
  float emissive_strength{0.0f};
  graphics::image2d_handle emissive_image{};
  math::vector2 size{100.0f, 100.0f};
  math::vector2 pivot{0.5f, 0.5f};
  math::vector2 position{0.0f, 0.0f};
  std::int32_t sort_order{0};
}; // struct screen_overlay_sprite

struct screen_camera_sprite {
  math::color base_color{1.0f, 1.0f, 1.0f, 1.0f};
  graphics::image2d_handle albedo_image{};
  math::color emissive_factor{0.0f, 0.0f, 0.0f, 1.0f};
  float emissive_strength{0.0f};
  graphics::image2d_handle emissive_image{};
  math::vector2 size{100.0f, 100.0f};
  math::vector2 pivot{0.5f, 0.5f};
  math::vector2 position{0.0f, 0.0f};
  float depth{0.0f};
  std::int32_t sort_order{0};
}; // struct screen_camera_sprite

struct world_sprite {
  math::color base_color{1.0f, 1.0f, 1.0f, 1.0f};
  graphics::image2d_handle albedo_image{};
  math::color emissive_factor{0.0f, 0.0f, 0.0f, 1.0f};
  float emissive_strength{0.0f};
  graphics::image2d_handle emissive_image{};
  math::vector2 size{1.0f, 1.0f};
  math::vector2 pivot{0.5f, 0.5f};
  bool is_billboard{false};
}; // struct world_sprite

using sprite = std::variant<screen_overlay_sprite, screen_camera_sprite, world_sprite>;

[[nodiscard]] inline auto get_sprite_space(const sprite& sprite) -> sprite_space {
  return static_cast<sprite_space>(sprite.index());
}

class sprite_batch {

public:

  inline static constexpr auto flag_is_billboard = 0x00000001u;
  inline static constexpr auto flag_msdf_text = 0x00000002u;

  struct sprite_instance {
    math::matrix4x4 model;
    math::color base_color;
    math::color emissive_factor;
    math::vector2 size;
    math::vector2 pivot;
    math::vector2 uv_min;
    math::vector2 uv_max;
    std::float_t emissive_strength;
    std::uint32_t albedo_image_index;
    std::uint32_t emissive_image_index;
    std::int32_t sort_order;
    std::uint32_t flags;
    std::uint32_t padding0;
    std::uint32_t padding1;
    std::uint32_t padding2;
  }; // struct sprite_instance

  sprite_batch() = default;

  auto clear() -> void {
    _instances.clear();
  }

  auto add(const sprite_instance& instance) -> void {
    _instances.push_back(instance);
  }

  auto sort() -> void {
    std::sort(_instances.begin(), _instances.end(), [](const sprite_instance& a, const sprite_instance& b) {
      return a.sort_order < b.sort_order;
    });
  }

  [[nodiscard]] auto instances() const -> const std::vector<sprite_instance>& {
    return _instances;
  }

  [[nodiscard]] auto size() const -> std::size_t {
    return _instances.size();
  }

  [[nodiscard]] auto is_empty() const -> bool {
    return _instances.empty();
  }

  [[nodiscard]] auto data() const -> const sprite_instance* {
    return _instances.data();
  }

  [[nodiscard]] auto byte_size() const -> std::size_t {
    return _instances.size() * sizeof(sprite_instance);
  }

private:

  std::vector<sprite_instance> _instances;

}; // class sprite_batch

class sprites_module : public core::module<sprites_module> {

  inline static constexpr auto mode_count = magic_enum::enum_count<sprite_space>();

  inline static const auto is_registered = register_module(stage::normal, dependencies<scenes::scenes_module>{});

public:

  sprites_module() = default;

  ~sprites_module() override = default;

  auto update() -> void override {
    for (auto& batch : _batches) {
      batch.clear();
    }

    _images.clear();

    _collect_sprites();
  }

  auto submit(sprite_space space, const sprite_batch::sprite_instance& instance) -> void {
    _batches[utility::to_underlying(space)].add(instance);
  }

  [[nodiscard]] auto register_image(graphics::image2d_handle image) -> std::uint32_t {
    return _images.push_back(image);
  }

  [[nodiscard]] auto batch(sprite_space space) -> sprite_batch& {
    return _batches[utility::to_underlying(space)];
  }

  [[nodiscard]] auto batch(sprite_space space) const -> const sprite_batch& {
    return _batches[utility::to_underlying(space)];
  }

  [[nodiscard]] auto images() -> graphics::separate_image2d_array& {
    return _images;
  }

  [[nodiscard]] auto images() const -> const graphics::separate_image2d_array& {
    return _images;
  }

private:

  auto _collect_sprites() -> void {
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
            .padding0 = 0u,
            .padding1 = 0u,
            .padding2 = 0u
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
            .padding0 = 0u,
            .padding1 = 0u,
            .padding2 = 0u
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
            .padding0 = 0u,
            .padding1 = 0u,
            .padding2 = 0u
          });
        }
      }, sprite);
    }
  }

  std::array<sprite_batch, mode_count> _batches;
  graphics::separate_image2d_array _images;

}; // class sprites_module

} // namespace sbx::sprites

#endif // LIBSBX_SPRITES_SPRITES_MODULE_HPP_
