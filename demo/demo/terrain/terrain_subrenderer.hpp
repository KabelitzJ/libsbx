#ifndef DEMO_TERRAIN_TERRAIN_SUBRENDERER_HPP_
#define DEMO_TERRAIN_TERRAIN_SUBRENDERER_HPP_

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/math/matrix_cast.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <demo/terrain/vertex.hpp>
#include <demo/terrain/pipeline.hpp>
#include <demo/terrain/mesh.hpp>
#include <demo/terrain/planet_generator_task.hpp>
#include <demo/terrain/chunk.hpp>

namespace demo {

class terrain_subrenderer final : public sbx::graphics::subrenderer {

  using base = sbx::graphics::subrenderer;

public:

  terrain_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path)
  : base{},
    _pipeline{path, attachments},
    _push_handler{_pipeline},
    _descriptor_handler{_pipeline, 0u} { }

  ~terrain_subrenderer() override = default;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override {
    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    auto& scene = scenes_module.scene();

    auto camera_node = scene.camera();

    auto& camera = scene.get_component<sbx::scenes::camera>(camera_node);

    _scene_uniform_handler.push("projection", camera.projection());

    const auto& camera_transform = scene.get_component<sbx::scenes::transform>(camera_node);

    _scene_uniform_handler.push("view", sbx::math::matrix4x4::inverted(scene.world_transform(camera_node)));

    auto chunk_query = scene.query<demo::chunk>();

    _pipeline.bind(command_buffer);

    for (const auto node : chunk_query) {
      const auto& chunk = scene.get_component<demo::chunk>(node);
      const auto& mesh = assets_module.get_asset<sbx::models::mesh>(chunk.mesh_id);

      const auto model = scene.world_transform(node);
      const auto normal = sbx::math::matrix4x4::transposed(sbx::math::matrix4x4::inverted(model));

      _push_handler.push("model", model);
      _push_handler.push("normal", normal);
      _push_handler.push("water_color", chunk.water_color);
      _push_handler.push("land_color", chunk.land_color);
      _push_handler.push("mountain_color", chunk.mountain_color);

      _descriptor_handler.push("scene", _scene_uniform_handler);
      // _descriptor_handler.push("push", _push_handler);

      if (!_descriptor_handler.update(_pipeline)) {
        return;
      }

      _descriptor_handler.bind_descriptors(command_buffer);
      _push_handler.bind(command_buffer);

      mesh.bind(command_buffer);
      mesh.render(command_buffer);
    }

  }

private:

  pipeline _pipeline;

  sbx::graphics::uniform_handler _scene_uniform_handler;
  sbx::graphics::push_handler _push_handler;
  sbx::graphics::descriptor_handler _descriptor_handler;
  sbx::graphics::storage_handler _storage_handler;

}; // class terrain_subrenderer

} // namespace demo::terrain

#endif // DEMO_TERRAIN_TERRAIN_SUBRENDERER_HPP_
