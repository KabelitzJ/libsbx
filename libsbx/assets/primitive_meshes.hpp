// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_PRIMITIVE_MESHES_HPP_
#define LIBSBX_ASSETS_PRIMITIVE_MESHES_HPP_

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/math/uuid.hpp>

namespace sbx::assets {

enum class [[=reflection::named]] primitive_mesh_kind : std::uint8_t {
  cube,
  sphere,
  plane,
  capsule,
  cylinder
}; // enum class primitive_mesh_kind

[[nodiscard]] auto primitive_mesh_uuid(const primitive_mesh_kind kind) -> math::uuid;

[[nodiscard]] auto primitive_mesh_name(const primitive_mesh_kind kind) -> std::string_view;

/**
 * @brief The primitive kind @p id refers to.
 *
 * @param id The mesh uuid.
 *
 * @return The kind, or nullopt if @p id isn't a built-in primitive.
 */
[[nodiscard]] auto primitive_mesh_kind_of(const math::uuid& id) -> std::optional<primitive_mesh_kind>;

/**
 * @brief Writes the primitive's cooked mesh and the default material if they aren't on disk yet; cheap to call every time.
 *
 * @param kind The primitive to cook.
 */
auto ensure_primitive_mesh_cooked(const primitive_mesh_kind kind) -> void;

/**
 * @brief The plain grey material built-in primitives are cooked with; a real uuid, so it survives scene serialization.
 *
 * @return The material's uuid.
 */
[[nodiscard]] auto default_material_uuid() -> math::uuid;

/** @brief Writes the default material's cooked blob if it isn't on disk yet; cheap to call every time. */
auto ensure_default_material_cooked() -> void;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_PRIMITIVE_MESHES_HPP_
