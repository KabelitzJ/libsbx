// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GIZMOS_MESH_SUBRENDERER_HPP_
#define LIBSBX_GIZMOS_MESH_SUBRENDERER_HPP_

#include <filesystem>
#include <unordered_set>
#include <ranges>
#include <algorithm>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>
#include <libsbx/graphics/buffers/uniform_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>
#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/gizmo.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/point_light.hpp>

#include <libsbx/models/mesh.hpp>

#include <libsbx/gizmos/pipeline.hpp>

namespace sbx::gizmos {

// struct point_light {
//   math::color color;
//   math::vector3 position;
//   std::float_t intensity;
// }; // struct point_light

class gizmos_subrenderer final : public graphics::subrenderer {

  inline static constexpr auto max_point_lights = std::size_t{16};

public:

  gizmos_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, const std::string& depth_image)
  : graphics::subrenderer{},
    _pipeline{path, attachments},
    _depth_image{depth_image} { }

  ~gizmos_subrenderer() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    auto& assets_module = core::engine::get_module<assets::assets_module>();
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& devices_module = core::engine::get_module<devices::devices_module>();

    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto camera_node = scene.camera();

    auto& camera = scene.get_component<scenes::camera>(camera_node);

    _scene_uniform_handler.push("projection", camera.projection());

    const auto& camera_transform = scene.get_component<scenes::transform>(camera_node);

    _scene_uniform_handler.push("view", math::matrix4x4::inverted(camera_transform.local_transform()));

    auto& window = devices_module.window();

    _scene_uniform_handler.push("resolution", math::vector2{window.width(), window.height()});

    _scene_uniform_handler.push("camera_position", camera_transform.position());

    for (auto entry = _uniform_data.begin(); entry != _uniform_data.end();) {
      if (_used_uniforms.contains(entry->first)) {
        ++entry;
      } else {
        entry = _uniform_data.erase(entry);
      }
    }

    _used_uniforms.clear();
    _static_meshes.clear();

    auto gizmo_query = scene.query<scenes::gizmo>();

    for (const auto node : gizmo_query) {
      _submit_mesh(node);
    }

    _pipeline.bind(command_buffer);
    
    for (const auto& [key, data] : _static_meshes) {

      auto [entry, inserted] = _uniform_data.try_emplace(key, 1u);

      auto& descriptor_handler = entry->second.descriptor_handler;
      auto& storage_handler = entry->second.storage_handler;

      storage_handler.push(std::span<const per_mesh_data>{data});

      auto& mesh = assets_module.get_asset<models::mesh>(key.mesh_id);

      descriptor_handler.push("scene", _scene_uniform_handler);
      descriptor_handler.push("buffer_mesh_data", storage_handler);
      descriptor_handler.push("depth_image", graphics_module.attachment(_depth_image));
      descriptor_handler.push("texture_image", assets_module.get_asset<graphics::image2d>(key.texture_id));

      if (!descriptor_handler.update(_pipeline)) {
        continue;
      }

      descriptor_handler.bind_descriptors(command_buffer);

      mesh.bind(command_buffer);
      mesh.render_submesh(command_buffer, key.submesh_index, static_cast<std::uint32_t>(data.size()));
    }
  }

private:

  auto _submit_mesh(const scenes::node node) -> void {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    const auto& gizmo = scene.get_component<scenes::gizmo>(node);
    const auto mesh_id = gizmo.mesh_id();

    const auto key = mesh_key{gizmo.mesh_id(), gizmo.submesh_index(), gizmo.texture_id()};

    _used_uniforms.insert(key);

    auto model = scene.world_transform(node);
    auto normal = math::matrix4x4::transposed(math::matrix4x4::inverted(model));

    _static_meshes[key].push_back(per_mesh_data{std::move(model), std::move(normal), gizmo.tint()});
  }

  struct uniform_data {
    graphics::descriptor_handler descriptor_handler;
    graphics::storage_handler storage_handler;
  }; // struct uniform_data

  struct mesh_key {
    math::uuid mesh_id;
    std::uint32_t submesh_index;
    math::uuid texture_id;
  }; // struct mesh_key

  struct per_mesh_data {
    alignas(16) math::matrix4x4 model;
    alignas(16) math::matrix4x4 normal;
    alignas(16) math::color tint;
  }; // struct per_mesh_data

  struct mesh_key_hash {
    auto operator()(const mesh_key& key) const noexcept -> std::size_t {
      auto seed = std::size_t{0};

      utility::hash_combine(seed, key.mesh_id, key.submesh_index);

      return seed;
    }
  }; // struct mesh_key_hash

  struct mesh_key_equal {
    auto operator()(const mesh_key& lhs, const mesh_key& rhs) const noexcept -> bool {
      return lhs.mesh_id == rhs.mesh_id && lhs.submesh_index == rhs.submesh_index;
    }
  }; // struct mesh_key_equal

  pipeline _pipeline;

  std::string _depth_image;

  std::unordered_map<mesh_key, uniform_data, mesh_key_hash, mesh_key_equal> _uniform_data;
  std::unordered_set<mesh_key, mesh_key_hash, mesh_key_equal> _used_uniforms;

  std::unordered_map<mesh_key, std::vector<per_mesh_data>, mesh_key_hash, mesh_key_equal> _static_meshes;

  graphics::uniform_handler _scene_uniform_handler;

}; // class gizmos_subrenderer

} // namespace sbx::giszmos

#endif // LIBSBX_GIZMOS_MESH_SUBRENDERER_HPP_
