// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/surface_raycast.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_map>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/raycast.hpp>

namespace editor {

// Main thread only, like everything in the editor; the generations each mesh's data was built at.
struct surface_cache {
  sbx::physics::mesh_collision_cache meshes{};
  std::unordered_map<sbx::math::uuid, std::uint64_t> generations{};
}; // struct surface_cache

auto surface_mesh_cache() -> surface_cache& {
  static auto cache = surface_cache{};

  return cache;
}

auto surface_mesh_data(const sbx::assets::mesh_handle& mesh) -> const sbx::physics::mesh_collision_data& {
  auto& cache = surface_mesh_cache();
  const auto id = mesh->id();

  // The physics cache never rebuilds one mesh, so a reimported mesh (its generation moved) clears it all; reimports are rare.
  if (const auto entry = cache.generations.find(id); entry != cache.generations.end() && entry->second != mesh.generation()) {
    cache.meshes.clear();
    cache.generations.clear();
  }

  cache.generations.insert_or_assign(id, mesh.generation());

  return cache.meshes.get_or_build(sbx::core::engine::get_module<sbx::assets::assets_module>(), id);
}

// Whether the node or one of its ancestors is ignored.
auto is_ignored(sbx::scenes::scene& scene, sbx::scenes::node node, std::span<const sbx::math::uuid> ignored) -> bool {
  if (ignored.empty()) {
    return false;
  }

  for (auto current = node; current.is_valid() && current.has_component<sbx::scenes::id>(); current = scene.node_of(current.get_component<sbx::scenes::relationship>().parent)) {
    if (std::ranges::contains(ignored, static_cast<sbx::math::uuid>(current.id()))) {
      return true;
    }
  }

  return false;
}

auto raycast_surface(sbx::scenes::scene& scene, const sbx::math::ray& ray, std::span<const sbx::math::uuid> ignored) -> std::optional<surface_hit> {
  auto best = std::optional<surface_hit>{};
  auto best_distance = std::numeric_limits<std::float_t>::max();

  for (auto&& [entity, renderer, world] : scene.query<sbx::scenes::mesh_renderer, sbx::scenes::world_transform>(sbx::ecs::exclude<sbx::scenes::inactive>).each()) {
    if (!renderer.mesh.is_valid()) {
      continue;
    }

    // The bounds reject most meshes before any triangle is touched; intersects() gives a ray parameter, hits a distance.
    const auto bounds_hit = sbx::math::volume::transformed(renderer.mesh->bounds(), world.matrix).intersects(ray);

    if (!bounds_hit || *bounds_hit * ray.direction().length() > best_distance) {
      continue;
    }

    auto node = scene.node_of(entity);

    if (is_ignored(scene, node, ignored)) {
      continue;
    }

    const auto& data = surface_mesh_data(renderer.mesh);

    if (data.indices.empty()) {
      continue;
    }

    const auto parts = sbx::math::decompose(world.matrix);
    const auto pose = sbx::physics::transform{parts.position, parts.rotation, parts.scale};

    if (const auto hit = sbx::physics::raycast_triangle_mesh(data, pose, ray, best_distance); hit && hit->distance < best_distance) {
      best_distance = hit->distance;
      best = surface_hit{node, hit->point, hit->normal, hit->distance};
    }
  }

  for (auto&& [entity, collider] : scene.query<sbx::physics::heightfield_collider>(sbx::ecs::exclude<sbx::scenes::inactive>).each()) {
    if (!collider.data) {
      continue;
    }

    auto node = scene.node_of(entity);

    if (is_ignored(scene, node, ignored)) {
      continue;
    }

    if (const auto hit = sbx::physics::raycast_heightfield(*collider.data, ray, best_distance); hit && hit->distance < best_distance) {
      best_distance = hit->distance;
      best = surface_hit{node, hit->point, hit->normal, hit->distance};
    }
  }

  return best;
}

} // namespace editor
