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
    std::float_t sdf_px_range;
    std::uint32_t padding0;
    std::uint32_t padding1;
  }; // struct sprite_instance

  sprite_batch() = default;

  auto clear() -> void;

  auto add(const sprite_instance& instance) -> void;

  auto sort() -> void;

  [[nodiscard]] auto instances() const -> const std::vector<sprite_instance>&;

  [[nodiscard]] auto size() const -> std::size_t;

  [[nodiscard]] auto is_empty() const -> bool;

  [[nodiscard]] auto data() const -> const sprite_instance*;

  [[nodiscard]] auto byte_size() const -> std::size_t;

private:

  std::vector<sprite_instance> _instances;

}; // class sprite_batch

class sprites_module : public core::module<sprites_module> {

  inline static constexpr auto mode_count = magic_enum::enum_count<sprite_space>();

  inline static const auto is_registered = register_module(stage::normal, dependencies<scenes::scenes_module>{});

public:

  sprites_module();

  ~sprites_module() override = default;

  auto update() -> void override;

  auto submit(sprite_space space, const sprite_batch::sprite_instance& instance) -> void;

  [[nodiscard]] auto register_image(graphics::image2d_handle image) -> std::uint32_t;

  [[nodiscard]] auto register_image(const std::string& attachment) -> std::uint32_t;

  [[nodiscard]] auto batch(sprite_space space) -> sprite_batch&;

  [[nodiscard]] auto batch(sprite_space space) const -> const sprite_batch&;

  [[nodiscard]] auto images() -> graphics::separate_image2d_array&;

  [[nodiscard]] auto images() const -> const graphics::separate_image2d_array&;

private:

  auto _collect_sprites() -> void;

  std::array<sprite_batch, mode_count> _batches;
  graphics::separate_image2d_array _images;

}; // class sprites_module

} // namespace sbx::sprites

#endif // LIBSBX_SPRITES_SPRITES_MODULE_HPP_
