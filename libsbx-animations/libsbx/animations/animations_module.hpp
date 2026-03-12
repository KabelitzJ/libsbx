// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ANIMATIONS_ANIMATIONS_MODULE_HPP_
#define LIBSBX_ANIMATIONS_ANIMATIONS_MODULE_HPP_

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scenes/components/skinned_mesh.hpp>

#include <libsbx/animations/animator.hpp>
#include <libsbx/animations/animation.hpp>
#include <libsbx/animations/mesh.hpp>

namespace sbx::animations {
  
class animations_module : public core::module<animations_module> {

  inline static const auto is_registered = register_module(stage::post);

public:

  animations_module();

  ~animations_module() override;

  auto update() -> void override;

  template<typename... Args>
  auto add_animated_mesh(const scenes::node node, const math::uuid mesh_id, Args&&... args) -> scenes::skinned_mesh& {
    auto& assets_module = core::engine::get_module<assets::assets_module>();
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

    auto& skinned_mesh = graph.add_component<sbx::scenes::skinned_mesh>(node, mesh_id, std::forward<Args>(args)...);

    const auto& mesh = assets_module.get_asset<animations::mesh>(mesh_id);

    const auto& skeleton = mesh.skeleton();

    const auto& bones = skeleton.bones();

    auto nodes = std::vector<scenes::node>{};
    nodes.reserve(bones.size());

    for (auto i = 0u; i < bones.size(); ++i) {
      const auto parent_id = bones[i].parent_id;
      const auto parent = (parent_id == skeleton::bone::null) ? node : nodes.at(parent_id);

      const auto [position, rotation, scale] = math::decompose(bones[i].local_bind_matrix);

      nodes.push_back(graph.create_child_node(parent, skeleton.name_for_bone(i).str(), scenes::transform{position, rotation, scale}));
    }

    skinned_mesh.set_nodes(std::move(nodes));

    return skinned_mesh;
  }

  auto find_skeleton_node(const scenes::node node, const utility::hashed_string& name) const -> scenes::node;

}; // class assets_module

} // namespace sbx::animations

#endif // LIBSBX_ANIMATIONS_ANIMATIONS_MODULE_HPP_