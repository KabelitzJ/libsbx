// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_DEBUG_SUBRENDERER_HPP_
#define LIBSBX_SCENES_DEBUG_SUBRENDERER_HPP_

#include <optional>

#include <libsbx/math/vector4.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/render_graph.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/scenes/scenes_module.hpp>

namespace sbx::scenes {

class debug_subrenderer final : public sbx::graphics::subrenderer {

  inline static const auto pipeline_definition = sbx::graphics::pipeline_definition{
    .depth = sbx::graphics::depth::disabled,
    .uses_transparency = false,
    .rasterization_state = sbx::graphics::rasterization_state{
      .polygon_mode = sbx::graphics::polygon_mode::fill,
      .cull_mode = sbx::graphics::cull_mode::none,
      .front_face = sbx::graphics::front_face::counter_clockwise
    },
    .primitive_topology = sbx::graphics::primitive_topology::line_list
  };

  inline static constexpr auto default_shader_path = std::string_view{"engine://shaders/debug"};

public:

  debug_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path = default_shader_path)
  : sbx::graphics::subrenderer{},
    _pipeline{path, attachments, pipeline_definition},
    _push_handler{_pipeline},
    _descriptor_handler{_pipeline, 0u} {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    _storage_buffer = graphics_module.add_resource<graphics::storage_buffer>(graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  }

  ~debug_subrenderer() override {

  }

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override {
    SBX_PROFILE_SCOPE("debug_subrenderer::render");

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    const auto& lines = scenes_module.debug_lines();

    if (lines.empty()) {
      return;
    }

    auto& scene = scenes_module.active_scene();
    auto& environment = scene.environment();
    auto& graph = scene.graph();
    
    _pipeline.bind(command_buffer);

    auto& storage_buffer = graphics_module.get_resource<graphics::storage_buffer>(_storage_buffer);

    const auto required_size = static_cast<std::uint32_t>(lines.size() * sizeof(scenes_module::line));

    if (storage_buffer.size() < required_size) {
      storage_buffer.resize(static_cast<std::size_t>(static_cast<std::float_t>(required_size) * 1.5f));
    }

    storage_buffer.update(lines.data(), required_size);

    _push_handler.push("vertices", storage_buffer.address());

    _descriptor_handler.push("scene", environment.uniform_handler());

    if (!_descriptor_handler.update(_pipeline)) {
      return;
    }

    _descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw(static_cast<std::uint32_t>(lines.size()), 1u, 0u, 0u);

    scenes_module.clear_debug_lines();
  }

private:

  graphics::graphics_pipeline _pipeline;

  sbx::graphics::push_handler _push_handler;

  // std::optional<graphics::storage_buffer> _storage_buffer;
  graphics::storage_buffer_handle _storage_buffer;

  sbx::graphics::descriptor_handler _descriptor_handler;

}; // class debug_subrenderer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_DEBUG_SUBRENDERER_HPP_
