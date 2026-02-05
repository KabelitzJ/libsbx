// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SPRITES_SPRITE_SUBRENDERER_HPP_
#define LIBSBX_SPRITES_SPRITE_SUBRENDERER_HPP_

#include <unordered_map>
#include <algorithm>
#include <array>

#include <magic_enum/magic_enum.hpp>

#include <libsbx/utility/fast_mod.hpp>
#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/overload.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/devices/devices_module.hpp>
#include <libsbx/devices/window.hpp>

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
  screen_overlay = 0,  // UI elements rendered on top of everything (no depth)
  screen_camera = 1,   // Screen-space but respects camera depth
  world = 2            // Full 3D world-space sprites (billboards, etc.)
}; // enum class sprite_space

struct screen_overlay_sprite {
  math::color base_color{1.0f, 1.0f, 1.0f, 1.0f};
  graphics::image2d_handle albedo_image{};
  math::color emissive_factor{0.0f, 0.0f, 0.0f, 1.0f};
  float emissive_strength{0.0f};
  graphics::image2d_handle emissive_image{};
  math::vector2 size{100.0f, 100.0f};        // pixels
  math::vector2 pivot{0.5f, 0.5f};
  math::vector2 position{0.0f, 0.0f};        // screen pixels from center
  std::int32_t sort_order{0};
}; // struct screen_overlay_sprite

struct screen_camera_sprite {
  math::color base_color{1.0f, 1.0f, 1.0f, 1.0f};
  graphics::image2d_handle albedo_image{};
  math::color emissive_factor{0.0f, 0.0f, 0.0f, 1.0f};
  float emissive_strength{0.0f};
  graphics::image2d_handle emissive_image{};
  math::vector2 size{100.0f, 100.0f};        // pixels
  math::vector2 pivot{0.5f, 0.5f};
  math::vector2 position{0.0f, 0.0f};        // screen pixels from center
  float depth{0.0f};                          // world units for occlusion
  std::int32_t sort_order{0};
}; // struct screen_camera_sprite

struct world_sprite {
  math::color base_color{1.0f, 1.0f, 1.0f, 1.0f};
  graphics::image2d_handle albedo_image{};
  math::color emissive_factor{0.0f, 0.0f, 0.0f, 1.0f};
  float emissive_strength{0.0f};
  graphics::image2d_handle emissive_image{};
  math::vector2 size{1.0f, 1.0f};            // world units
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

  struct sprite_instance {
    math::matrix4x4 model;
    math::color base_color;
    math::color emissive_factor;
    math::vector2 size;
    math::vector2 pivot;
    std::float_t emissive_strength;
    std::uint32_t albedo_image_index;
    std::uint32_t emissive_image_index;
    std::int32_t sort_order;
    std::uint32_t flags;
    std::uint32_t padding0;
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

class sprite_subrenderer : public graphics::subrenderer {

  static constexpr auto mode_count = magic_enum::enum_count<sprite_space>();

  struct pipeline_key {
    sprite_space space;

    auto operator==(const pipeline_key& other) const -> bool = default;
  }; // struct pipeline_key

  struct pipeline_key_hash {
    auto operator()(const pipeline_key& key) const -> std::size_t {
      return std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(key.space));
    }
  }; // struct pipeline_key_hash

  struct pipeline_data {
    graphics::graphics_pipeline_handle pipeline;
    graphics::push_handler push_handler;
    graphics::descriptor_handler descriptor_handler;

    pipeline_data(const graphics::graphics_pipeline_handle& handle)
    : pipeline{handle},
      push_handler{handle},
      descriptor_handler{handle, 0u} { }

  }; // struct pipeline_data

public:

  sprite_subrenderer(const memory::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path)
  : graphics::subrenderer{},
    _attachments{attachments},
    _shader_path{path},
    _sampler{} {
    
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    for (std::size_t i = 0; i < mode_count; ++i) {
      _sprite_buffers[i] = graphics_module.add_resource<graphics::storage_buffer>(
        graphics::storage_buffer::min_size, 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
      );
    }
  }

  ~sprite_subrenderer() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    SBX_PROFILE_SCOPE("sprite_subrenderer::render");

    _images.clear();
  
    for (auto& batch : _batches) {
      batch.clear();
    }

    _collect_sprites();

    for (auto& batch : _batches) {
      batch.sort();
    }

    // Render each mode in order: World -> Camera -> Overlay
    _render_batch(command_buffer, sprite_space::world);
    _render_batch(command_buffer, sprite_space::screen_camera);
    _render_batch(command_buffer, sprite_space::screen_overlay);
  }

private:

  auto _collect_sprites() -> void {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto sprite_query = scene.query<const sprites::sprite>();

    for (auto&& [node, sprite] : sprite_query.each()) {
      std::visit(utility::overload{
        [&](const screen_overlay_sprite& sprite){
          const auto albedo_index = _images.push_back(sprite.albedo_image);
          const auto emissive_index = _images.push_back(sprite.emissive_image);

          _batches[0].add(sprite_batch::sprite_instance{
            .model = math::matrix4x4::translated(math::matrix4x4::identity, math::vector3{sprite.position.x(), sprite.position.y(), 0.0f}),
            .base_color = sprite.base_color,
            .emissive_factor = sprite.emissive_factor,
            .size = sprite.size,
            .pivot = sprite.pivot,
            .emissive_strength = sprite.emissive_strength,
            .albedo_image_index = albedo_index,
            .emissive_image_index = emissive_index,
            .sort_order = sprite.sort_order,
            .flags = 0u,
            .padding0 = 0u
          });
        },
        [&](const screen_camera_sprite& sprite){
          const auto albedo_index = _images.push_back(sprite.albedo_image);
          const auto emissive_index = _images.push_back(sprite.emissive_image);

          _batches[1].add(sprite_batch::sprite_instance{
            .model = math::matrix4x4::translated(math::matrix4x4::identity, math::vector3{sprite.position.x(), sprite.position.y(), sprite.depth}),
            .base_color = sprite.base_color,
            .emissive_factor = sprite.emissive_factor,
            .size = sprite.size,
            .pivot = sprite.pivot,
            .emissive_strength = sprite.emissive_strength,
            .albedo_image_index = albedo_index,
            .emissive_image_index = emissive_index,
            .sort_order = sprite.sort_order,
            .flags = 0u,
            .padding0 = 0u
          });
        },
        [&](const world_sprite& sprite){
          const auto albedo_index = _images.push_back(sprite.albedo_image);
          const auto emissive_index = _images.push_back(sprite.emissive_image);

          const auto& transform = scene.world_transform(node);

          auto flags = std::uint32_t{0u};

          if (sprite.is_billboard) {
            flags |= sprite_batch::flag_is_billboard;
          }

          _batches[2].add(sprite_batch::sprite_instance{
            .model = transform,
            .base_color = sprite.base_color,
            .emissive_factor = sprite.emissive_factor,
            .size = sprite.size,
            .pivot = sprite.pivot,
            .emissive_strength = sprite.emissive_strength,
            .albedo_image_index = albedo_index,
            .emissive_image_index = emissive_index,
            .sort_order = 0,
            .flags = flags,
            .padding0 = 0u
          });
        }
      }, sprite);
    }
  }

  auto _get_or_create_pipeline(sprite_space space) -> pipeline_data& {
    const auto key = pipeline_key{space};

    if (auto entry = _pipeline_cache.find(key); entry != _pipeline_cache.end()) {
      return entry->second;
    }

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& compiler = graphics_module.compiler();

    auto definition = graphics::pipeline_definition{
      .uses_transparency = true,
      .rasterization_state = graphics::rasterization_state{
        .polygon_mode = graphics::polygon_mode::fill,
        .cull_mode = graphics::cull_mode::none,
        .front_face = graphics::front_face::counter_clockwise
      }
    };

    const auto request = graphics::compiler::compile_request{
      .path = _shader_path,
      .defines = {},
      .per_stage = {
        {SLANG_STAGE_VERTEX, {
          .entry_point = "main",
          .specializations = {_mode_specializations.at(utility::to_underlying(space))}
        }},
        {SLANG_STAGE_FRAGMENT, {
          .entry_point = "main"
        }}
      }
    };

    const auto result = compiler.compile(request);

    auto compiled_shaders = graphics::graphics_pipeline::compiled_shaders{
      _shader_path.filename().string(), 
      result.code
    };

    auto pipeline_handle = graphics_module.add_resource<graphics::graphics_pipeline>(compiled_shaders,  _attachments,  definition);

    auto [entry, inserted] = _pipeline_cache.emplace(key, pipeline_data{pipeline_handle});

    return entry->second;
  }

  auto _update_buffer(sprite_space space) -> graphics::storage_buffer& {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    const auto mode_index = utility::to_underlying(space);

    auto& buffer = graphics_module.get_resource<graphics::storage_buffer>(_sprite_buffers[mode_index]);

    const auto& batch = _batches[mode_index];

    if (batch.is_empty()) {
      return buffer;
    }

    const auto required_size = batch.byte_size();

    if (buffer.size() < required_size) {
      buffer.resize(static_cast<std::size_t>(static_cast<float>(required_size) * 1.5f));
    }

    buffer.update(batch.data(), static_cast<std::uint32_t>(required_size));

    return buffer;
  }

  auto _render_batch(graphics::command_buffer& command_buffer, sprite_space space) -> void {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    const auto mode_index = utility::to_underlying(space);

    if (_batches[mode_index].is_empty()) {
      return;
    }

    SBX_PROFILE_SCOPE("sprite_subrenderer::render_batch");

    auto& buffer = _update_buffer(space);
    auto& pipeline_data = _get_or_create_pipeline(space);
    auto& pipeline = graphics_module.get_resource<graphics::graphics_pipeline>(pipeline_data.pipeline);

    pipeline.bind(command_buffer);

    pipeline_data.descriptor_handler.push("scene", scene.uniform_handler());
    pipeline_data.descriptor_handler.push("images", _images);
    pipeline_data.descriptor_handler.push("sampler", _sampler);

    pipeline_data.push_handler.push("sprites", buffer.address());

    if (!pipeline_data.descriptor_handler.update(pipeline)) {
      return;
    }

    pipeline_data.descriptor_handler.bind_descriptors(command_buffer);
    pipeline_data.push_handler.bind(command_buffer);

    command_buffer.draw(6u, _batches[mode_index].size(), 0u, 0u);
  }

  inline static const auto _mode_specializations = std::array<std::string, 3u>{
    "screen_overlay_mode",  // sprite_space::screen_overlay
    "screen_camera_mode",   // sprite_space::screen_camera
    "world_space_mode"      // sprite_space::world
  };


  memory::vector<graphics::attachment_description> _attachments;
  std::filesystem::path _shader_path;

  memory::unordered_map<pipeline_key, pipeline_data, pipeline_key_hash> _pipeline_cache;

  std::array<graphics::storage_buffer_handle, mode_count> _sprite_buffers;
  std::array<sprite_batch, mode_count> _batches;

  graphics::separate_image2d_array _images;
  graphics::sampler_state _sampler;

}; // class sprite_subrenderer

} // namespace sbx::sprites

#endif // LIBSBX_SPRITES_SPRITE_SUBRENDERER_HPP_