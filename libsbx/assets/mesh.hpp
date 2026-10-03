// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_MESH_HPP_
#define LIBSBX_ASSETS_MESH_HPP_

#include <array>
#include <cstdint>
#include <vector>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/loadable.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/skeleton.hpp>
#include <libsbx/assets/animation_clip.hpp>

namespace sbx::assets {

/** @brief Interleaved vertex, scalar-packed to match the shader's scalar-layout buffer pointer. `color` defaults to opaque white. */
struct alignas(std::float_t) vertex {
  math::vector3 position;
  math::vector3 normal;
  math::vector2 uv;
  math::vector4 tangent;
  math::color color{};
}; // struct vertex

/**
 * @brief Per-vertex skin data in a separate array parallel to @ref vertex, keeping static meshes small and the skin data out of meshopt's float codec.
 *
 * Up to 4 joint influences widened to 32 bits: only shaderInt16 is enabled, not 16-bit storage, so 16-bit indices aren't readable via BDA. Weights sum to 1.
 */
struct alignas(std::float_t) skin_vertex {
  std::array<std::uint32_t, 4u> joint_indices;
  math::vector4 weights;
}; // struct skin_vertex

/** @brief A loaded mesh: one vertex and index buffer drawn as one or more submeshes, filled on the render thread and read in shaders via BDA. */
class mesh final : public loadable {

  friend class asset_residency;

public:

  /** @brief One coarser LOD level: an index range into the same vertex buffer as LOD0. */
  struct lod_level {
    std::uint32_t index_offset;
    std::uint32_t index_count;
    std::float_t error; // meshopt_simplify's relative error
  }; // struct lod_level

  struct submesh {
    std::uint32_t index_offset;
    std::uint32_t index_count;
    math::volume bounds;
    material_handle material;
    std::vector<lod_level> lods{}; // coarser levels beyond LOD0; may be empty. Not used by the renderer yet.
  }; // struct submesh

  mesh() = default;

  mesh(std::vector<submesh> submeshes, const math::volume& bounds, std::uint32_t vertex_count = 0u)
  : _submeshes{std::move(submeshes)}, _vertex_count{vertex_count}, _bounds{bounds} { }

  [[nodiscard]] auto bounds() const noexcept -> const math::volume& {
    return _bounds;
  }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return !_submeshes.empty();
  }

  [[nodiscard]] auto submeshes() const noexcept -> const std::vector<submesh>& {
    return _submeshes;
  }

  [[nodiscard]] auto vertex_address() const noexcept -> graphics::buffer::address_type {
    return _vertex_address;
  }

  /**
   * @brief Vertex count of the shared vertex buffer (LOD0), for sizing a skin dispatch.
   *
   * @return The vertex count.
   */
  [[nodiscard]] auto vertex_count() const noexcept -> std::uint32_t {
    return _vertex_count;
  }

  [[nodiscard]] auto index_buffer() const noexcept -> const graphics::buffer_handle& {
    return _index_buffer;
  }

  /**
   * @brief Whether the mesh has per-vertex joint indices and weights.
   *
   * @return True for skinned meshes.
   */
  [[nodiscard]] auto has_skin_data() const noexcept -> bool {
    return _skin_vertex_address != 0u;
  }

  /**
   * @brief Device address of the skin_vertex array, parallel to vertex_address().
   *
   * @return The address, or 0 without skin data.
   */
  [[nodiscard]] auto skin_vertex_address() const noexcept -> graphics::buffer::address_type {
    return _skin_vertex_address;
  }

  /**
   * @brief The skeleton cooked with this mesh.
   *
   * @return The skeleton, invalid without skin data.
   */
  [[nodiscard]] auto skeleton() const noexcept -> const skeleton_handle& {
    return _skeleton;
  }

  /**
   * @brief Clips cooked from the same glTF file, resolved against skeleton()'s joints.
   *
   * @return The clips, empty if unskinned.
   */
  [[nodiscard]] auto animation_clips() const noexcept -> const std::vector<animation_clip_handle>& {
    return _animation_clips;
  }

  [[nodiscard]] auto is_uploaded() const noexcept -> bool {
    return _uploaded;
  }

  [[nodiscard]] auto resident_frame() const noexcept -> std::uint64_t {
    return _resident_frame;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

private:

  // Called once on the render thread after the GPU buffers exist; skin buffers stay invalid without skin data.
  auto _finalize(graphics::buffer_handle vertex_buffer, graphics::buffer_handle index_buffer, graphics::buffer::address_type vertex_address, std::uint64_t resident_frame, graphics::buffer_handle skin_vertex_buffer = {}, graphics::buffer::address_type skin_vertex_address = 0u) -> void {
    _vertex_buffer = vertex_buffer;
    _index_buffer = index_buffer;
    _vertex_address = vertex_address;
    _skin_vertex_buffer = skin_vertex_buffer;
    _skin_vertex_address = skin_vertex_address;
    _resident_frame = resident_frame;
    _uploaded = true;
  }

  // CPU data only, so set directly before the mesh is resident.
  auto _set_skeletal_data(skeleton_handle skeleton, std::vector<animation_clip_handle> animation_clips) -> void {
    _skeleton = std::move(skeleton);
    _animation_clips = std::move(animation_clips);
  }

  // Fills a placeholder mesh once its cooked content arrives; called once on the main thread.
  auto _finalize_content(std::vector<submesh> submeshes, const math::volume& bounds, std::uint32_t vertex_count) -> void {
    _submeshes = std::move(submeshes);
    _bounds = bounds;
    _vertex_count = vertex_count;
  }

  std::vector<submesh> _submeshes{};
  graphics::buffer_handle _vertex_buffer{};
  graphics::buffer_handle _index_buffer{};
  graphics::buffer::address_type _vertex_address{0u};
  std::uint32_t _vertex_count{0u};
  graphics::buffer_handle _skin_vertex_buffer{};
  graphics::buffer::address_type _skin_vertex_address{0u};
  skeleton_handle _skeleton{};
  std::vector<animation_clip_handle> _animation_clips{};
  std::uint64_t _resident_frame{0u};
  bool _uploaded{false};
  math::volume _bounds{};
  math::uuid _id{math::uuid::nil()};

}; // class mesh

using mesh_handle = asset_handle<mesh>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_MESH_HPP_
