// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SPRITES_SPRITE_SUBRENDERER_HPP_
#define LIBSBX_SPRITES_SPRITE_SUBRENDERER_HPP_

#include <unordered_map>
#include <array>

#include <magic_enum/magic_enum.hpp>

#include <libsbx/utility/fast_mod.hpp>
#include <libsbx/utility/enum.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/pipeline/compiler.hpp>

#include <libsbx/graphics/images/image.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>
#include <libsbx/graphics/images/depth_image.hpp>

#include <libsbx/graphics/buffers/push_handler.hpp>
#include <libsbx/graphics/buffers/uniform_handler.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/sprites/sprites_module.hpp>

namespace sbx::sprites {

class sprite_subrenderer : public graphics::subrenderer {

  static constexpr auto mode_count = magic_enum::enum_count<sprite_space>();

  struct pipeline_key {
    sprite_space space;

    auto operator==(const pipeline_key& other) const -> bool = default;
  }; // struct pipeline_key

  struct pipeline_key_hash {
    auto operator()(const pipeline_key& key) const -> std::size_t;
  }; // struct pipeline_key_hash

  struct pipeline_data {
    graphics::graphics_pipeline_handle pipeline;
    graphics::push_handler push_handler;
    graphics::descriptor_handler descriptor_handler;

    pipeline_data(const graphics::graphics_pipeline_handle& handle);

  }; // struct pipeline_data

  inline static const auto default_shader_path = std::string_view{"engine://shaders/sprites"};

public:

  sprite_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path = default_shader_path);

  ~sprite_subrenderer() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override;

private:

  auto _get_or_create_pipeline(sprite_space space) -> pipeline_data&;

  auto _update_buffer(sprite_space space) -> graphics::storage_buffer&;

  auto _render_batch(graphics::command_buffer& command_buffer, sprite_space space) -> void;

  inline static const auto _mode_specializations = std::array<std::string, 3u>{
    "screen_overlay_mode",
    "screen_camera_mode",
    "world_space_mode"
  };

  std::vector<graphics::attachment_description> _attachments;
  std::filesystem::path _shader_path;

  std::unordered_map<pipeline_key, pipeline_data, pipeline_key_hash> _pipeline_cache;

  std::array<graphics::storage_buffer_handle, mode_count> _sprite_buffers;

  graphics::sampler_state _sampler;

}; // class sprite_subrenderer

} // namespace sbx::sprites

#endif // LIBSBX_SPRITES_SPRITE_SUBRENDERER_HPP_
