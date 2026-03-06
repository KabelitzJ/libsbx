// SPDX-License-Identifier: MIT
#include <libsbx/sprites/sprite_subrenderer.hpp>

namespace sbx::sprites {

auto sprite_subrenderer::pipeline_key_hash::operator()(const pipeline_key& key) const -> std::size_t {
  return std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(key.space));
}

sprite_subrenderer::pipeline_data::pipeline_data(const graphics::graphics_pipeline_handle& handle)
: pipeline{handle},
  push_handler{handle},
  descriptor_handler{handle, 0u} { }

sprite_subrenderer::sprite_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path)
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

auto sprite_subrenderer::render(graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("sprite_subrenderer::render");

  auto& sprites_module = core::engine::get_module<sprites::sprites_module>();

  for (std::size_t i = 0; i < mode_count; ++i) {
    sprites_module.batch(static_cast<sprite_space>(i)).sort();
  }

  _render_batch(command_buffer, sprite_space::world);
  _render_batch(command_buffer, sprite_space::screen_camera);
  _render_batch(command_buffer, sprite_space::screen_overlay);
}

auto sprite_subrenderer::_get_or_create_pipeline(sprite_space space) -> pipeline_data& {
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

  auto pipeline_handle = graphics_module.add_resource<graphics::graphics_pipeline>(compiled_shaders, _attachments, definition);

  auto [entry, inserted] = _pipeline_cache.emplace(key, pipeline_data{pipeline_handle});

  return entry->second;
}

auto sprite_subrenderer::_update_buffer(sprite_space space) -> graphics::storage_buffer& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& sprites_module = core::engine::get_module<sprites::sprites_module>();

  const auto mode_index = utility::to_underlying(space);

  auto& buffer = graphics_module.get_resource<graphics::storage_buffer>(_sprite_buffers[mode_index]);

  const auto& batch = sprites_module.batch(space);

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

auto sprite_subrenderer::_render_batch(graphics::command_buffer& command_buffer, sprite_space space) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& sprites_module = core::engine::get_module<sprites::sprites_module>();

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  if (sprites_module.batch(space).is_empty()) {
    return;
  }

  SBX_PROFILE_SCOPE("sprite_subrenderer::render_batch");

  auto& buffer = _update_buffer(space);
  auto& pipeline_data = _get_or_create_pipeline(space);
  auto& pipeline = graphics_module.get_resource<graphics::graphics_pipeline>(pipeline_data.pipeline);

  pipeline.bind(command_buffer);

  pipeline_data.descriptor_handler.push("scene", scene.uniform_handler());
  pipeline_data.descriptor_handler.push("images", sprites_module.images());
  pipeline_data.descriptor_handler.push("sampler", _sampler);

  pipeline_data.push_handler.push("sprites", buffer.address());

  if (!pipeline_data.descriptor_handler.update(pipeline)) {
    return;
  }

  pipeline_data.descriptor_handler.bind_descriptors(command_buffer);
  pipeline_data.push_handler.bind(command_buffer);

  command_buffer.draw(6u, sprites_module.batch(space).size(), 0u, 0u);
}

} // namespace sbx::sprites