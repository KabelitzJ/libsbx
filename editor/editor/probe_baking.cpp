// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/probe_baking.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>
#include <libsbx/render/probes/cubemap_capture.hpp>

#include <editor/commands/component_commands.hpp>

namespace editor {

// Where a probe's bake lands: one folder per scene, one file per probe node.
auto probe_bake_path(const sbx::scenes::scene& scene, const sbx::math::uuid& node_id, std::string_view kind, std::string_view extension) -> std::filesystem::path {
  return sbx::core::engine::project().assets_directory() / "baked" / scene.name() / fmt::format("{}_{}.{}", kind, node_id.value(), extension);
}

// Registers a freshly written bake, or re-cooks it in place when an earlier bake already imported it, so live handles pick up the new data.
auto import_bake(const std::filesystem::path& path) -> sbx::math::uuid {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (const auto known = assets_module.find(path); known != sbx::math::uuid::nil()) {
    assets_module.reimport(known);
    return known;
  }

  return assets_module.import(path);
}

auto bake_reflection_probe(editor_state& state, sbx::scenes::scene& scene, sbx::scenes::node node) -> void {
  const auto& probe = node.get_component<sbx::scenes::reflection_probe>();

  const auto node_id = static_cast<sbx::math::uuid>(node.id());
  const auto position = sbx::math::vector3{node.world_matrix()[3]};
  const auto path = probe_bake_path(scene, node_id, "reflection_probe", "hdr");

  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();

  scene_renderer_module.request_cubemap_capture(position, probe.resolution, probe.near_plane, probe.far_plane, [&state, node_id, path](sbx::render::cubemap_capture&& capture) {
    if (!sbx::render::save_cubemap_capture_hdr(capture, path)) {
      sbx::utility::logger<"editor">::error("Bake Reflection Probe: could not write '{}'", path.generic_string());
      return;
    }

    const auto id = import_bake(path);
    const auto environment = sbx::core::engine::get_module<sbx::assets::assets_module>().load_environment_map(id);

    auto& target = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();
    auto target_node = target.find(node_id);

    if (!target_node.is_valid() || !target_node.has_component<sbx::scenes::reflection_probe>()) {
      return;
    }

    auto& baked_probe = target_node.get_component<sbx::scenes::reflection_probe>();

    if (baked_probe.baked.is_valid() && baked_probe.baked->id() == id) {
      return;
    }

    const auto before = baked_probe;
    baked_probe.baked = environment;

    state.push_command(target, std::make_unique<modify_component_command<sbx::scenes::reflection_probe>>(node_id, before, baked_probe, "Bake Reflection Probe"));
  });
}

auto bake_all_reflection_probes(editor_state& state, sbx::scenes::scene& scene) -> void {
  for (auto&& [entity, probe] : scene.query<sbx::scenes::reflection_probe>(sbx::ecs::exclude<sbx::scenes::inactive>).each()) {
    bake_reflection_probe(state, scene, scene.node_of(entity));
  }
}

} // namespace editor
