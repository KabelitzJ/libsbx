// SPDX-License-Identifier: MIT
#include <libsbx/animations/animations_module.hpp>

namespace sbx::animations {

animations_module::animations_module() {
    
}

animations_module::~animations_module() {

}

auto animations_module::update() -> void {
  SBX_PROFILE_SCOPE("animations_module::update");

  auto& application = core::engine::get_application();

  if (application.is_paused()) {
    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();


  const auto delta_time = core::engine::delta_time();

  auto animator_query = graph.query<animator>();

  for (auto&& [node, animator] : animator_query.each()) {
    animator.update(delta_time);

    auto& skinned_mesh = graph.get_component<scenes::skinned_mesh>(node);

    const auto& mesh = assets_module.get_asset<animations::mesh>(skinned_mesh.mesh_id());

    const auto& skeleton = mesh.skeleton();

    auto locals = animator.evaluate_locals(skeleton);

    const auto& nodes = skinned_mesh.nodes();

    for (auto i = 0u; i < nodes.size(); ++i) {
      auto& transform = graph.get_component<scenes::transform>(nodes[i]);

      const auto& local = locals[i];

      transform.set_position(local.position);
      transform.set_rotation(local.rotation);
      transform.set_scale(local.scale);
    }

    skinned_mesh.set_pose(animator.evaluate_pose(skeleton, std::move(locals)));
  }
}

auto animations_module::find_skeleton_node(const scenes::node node, const utility::hashed_string& name) const -> scenes::node {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto& skinned_mesh = graph.get_component<sbx::scenes::skinned_mesh>(node);

  const auto& mesh = assets_module.get_asset<animations::mesh>(skinned_mesh.mesh_id());

  const auto& skeleton = mesh.skeleton();

  if (const auto index = skeleton.bone_index(name); index) {
    return skinned_mesh.find_node(*index);
  }

  return scenes::node::null;
}

} // namespace sbx::animations