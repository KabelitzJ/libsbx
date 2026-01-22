// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SPRITES_SPRITE_SUBRENDERER_HPP_
#define LIBSBX_SPRITES_SPRITE_SUBRENDERER_HPP_

#include <unordered_map>
#include <algorithm>

#include <libsbx/utility/fast_mod.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/devices/devices_module.hpp>
#include <libsbx/devices/window.hpp>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/images/image.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>
#include <libsbx/graphics/images/depth_image.hpp>

#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/graphics/buffers/uniform_handler.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/directional_light.hpp>
#include <libsbx/scenes/components/tag.hpp>

#include <libsbx/models/mesh.hpp>
#include <libsbx/models/vertex3d.hpp>
#include <libsbx/models/material.hpp>
#include <libsbx/models/material_draw_list.hpp>

namespace sbx::sprites {

enum class sprite_space : std::uint32_t {
  screen_overlay,
  screen_camera,
  world
}; // enum class sprite_space

struct sprite {
  sprite_space space;
  graphics::image2d_handle image;
  math::color color;
  std::uint32_t sort_key{0};
  bool is_billboard{false};
}; // struct sprite

class sprite_subrenderer : public graphics::subrenderer {

  class pipeline : public graphics::graphics_pipeline {

    inline static const auto pipeline_definition = graphics::pipeline_definition{
      .depth = graphics::depth::read_write,
      .uses_transparency = true,
      .rasterization_state = graphics::rasterization_state{
        .polygon_mode = graphics::polygon_mode::fill,
        .cull_mode = graphics::cull_mode::none,
        .front_face = graphics::front_face::counter_clockwise
      }
    };

    using base = graphics::graphics_pipeline;

  public:

    pipeline(const std::filesystem::path& path, const std::vector<graphics::attachment_description>& attachments)
    : base{path, attachments, pipeline_definition} { }

    ~pipeline() override = default;

  }; // class pipeline

public:

  sprite_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path)
  : graphics::subrenderer{},
    _pipeline{path, attachments},
    _push_handler{_pipeline},
    _descriptor_handler{_pipeline, 0u},
    _images{},
    _sampler{} {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    _sprite_data = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  }

  ~sprite_subrenderer() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    SBX_PROFILE_SCOPE("sprite_subrenderer::render");

    _images.clear();

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    _pipeline.bind(command_buffer);

    auto& sprite_data = graphics_module.get_resource<graphics::storage_buffer>(_sprite_data);

    auto sprites = std::vector<sprite_subrenderer::sprite_data>{};
    sprites.reserve(16u);

    auto sprite_query = scene.query<const sprites::sprite>();

    for (auto&& [node, sprite] : sprite_query.each()) {
      sprites.emplace_back(scene.world_transform(node), sprite.color, math::vector2{10, 10}, utility::to_underlying(sprite.space), _images.push_back(sprite.image));
    }

    const auto required_size = static_cast<std::uint32_t>(sprites.size() * sizeof(sprite_subrenderer::sprite_data));

    if (sprite_data.size() < required_size) {
      sprite_data.resize(static_cast<std::size_t>(static_cast<std::float_t>(required_size) * 1.5f));
    }

    sprite_data.update(sprites.data(), required_size);

    _descriptor_handler.push("scene", scene.uniform_handler());
    _descriptor_handler.push("images", _images);
    _descriptor_handler.push("sampler", _sampler);

    _push_handler.push("sprites", sprite_data.address());

    if (!_descriptor_handler.update(_pipeline)) {
      return;
    }

    _descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw(6u, sprites.size(), 0u, 0u);
  }

private:

  struct sprite_data {
    math::matrix4x4 model;
    math::color color;
    math::vector2 size;
    std::uint32_t space;
    std::uint32_t image_index;
  }; // struct sprite_data

  pipeline _pipeline;

  graphics::push_handler _push_handler;
  graphics::descriptor_handler _descriptor_handler;
  graphics::separate_image2d_array _images;
  graphics::sampler_state _sampler;
  graphics::storage_buffer_handle _sprite_data;

}; // class sprite_subrenderer

} // namespace sbx::sprites

#endif // LIBSBX_SPRITES_SPRITE_SUBRENDERER_HPP_
