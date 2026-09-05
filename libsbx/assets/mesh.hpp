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

#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/assets/asset_handle.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/skeleton.hpp>
#include <libsbx/assets/animation_clip.hpp>

namespace sbx::assets {

/**
 * @brief Interleaved vertex, scalar-packed (32 bytes) to match the shader's scalar-layout buffer pointer.
 */
struct alignas(std::float_t) vertex {
  math::vector3 position;
  math::vector3 normal;
  math::vector2 uv;
  math::vector4 tangent;
}; // struct vertex

/**
 * @brief Per-vertex skin data for a skinned mesh -- a separate array indexed 1:1 with @ref vertex,
 * not merged into it (keeps static meshes at 32 bytes/vertex, and the skin data unencoded --
 * meshopt's vertex codec targets quantizable floats, not packed joint indices).
 *
 * joint_indices holds up to 4 joint influences (glTF's JOINTS_0/WEIGHTS_0 convention), widened to
 * 32 bits -- the device doesn't enable VK_KHR_16bit_storage, only shaderInt16 (arithmetic, not
 * buffer layout), so a packed 16-bit index here wouldn't be safely readable via BDA. Weights are
 * renormalized to sum to 1.0 at cook time.
 */
struct alignas(std::float_t) skin_vertex {
  std::array<std::uint32_t, 4u> joint_indices;
  math::vector4 weights;
}; // struct skin_vertex

/**
 * @brief A loaded mesh: one device-local vertex + index buffer, drawn as one or more submeshes
 * (one per glTF primitive). The GPU buffers are filled on the render thread; the mesh is drawable
 * once resident. The vertex buffer is read in the shader via its device address (BDA).
 */
class mesh final {

  friend class asset_residency;

public:

  /** @brief One coarser level in a submesh's LOD chain — an index range into the same vertex buffer as its LOD0. */
  struct lod_level {
    std::uint32_t index_offset;
    std::uint32_t index_count;
    std::float_t error; // meshopt_simplify's relative error metric for this level
  }; // struct lod_level

  struct submesh {
    std::uint32_t index_offset;
    std::uint32_t index_count;
    math::volume bounds;
    material_handle material;
    std::vector<lod_level> lods{}; // progressively coarser levels beyond index_offset/index_count (LOD0); may be empty. Not yet consumed by the renderer — always drawn at LOD0.
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

  /** @brief Total vertex count across every submesh's shared vertex buffer (LOD0). Needed to size a skin dispatch over the whole instance. */
  [[nodiscard]] auto vertex_count() const noexcept -> std::uint32_t {
    return _vertex_count;
  }

  [[nodiscard]] auto index_buffer() const noexcept -> const graphics::buffer_handle& {
    return _index_buffer;
  }

  /** @brief Whether this mesh has per-vertex joint indices/weights (cooked from a glTF primitive with JOINTS_0/WEIGHTS_0). */
  [[nodiscard]] auto has_skin_data() const noexcept -> bool {
    return _skin_vertex_address != 0u;
  }

  /** @brief BDA of the parallel skin_vertex array, indexed 1:1 with vertex_address(); 0 if @ref has_skin_data is false. */
  [[nodiscard]] auto skin_vertex_address() const noexcept -> graphics::buffer::address_type {
    return _skin_vertex_address;
  }

  /** @brief Auto-populated by asset_residency::load_mesh when the cooked mesh carries skin data; invalid otherwise. */
  [[nodiscard]] auto skeleton() const noexcept -> const skeleton_handle& {
    return _skeleton;
  }

  /** @brief Clips cooked from the same glTF file's animations, resolved against @ref skeleton's joints. Empty if unskinned. */
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

  // Called once by asset_residency on the render thread, after the GPU buffers exist. skin_vertex_buffer/
  // skin_vertex_address stay default (invalid) for a mesh with no skin data.
  auto _finalize(graphics::buffer_handle vertex_buffer, graphics::buffer_handle index_buffer, graphics::buffer::address_type vertex_address, std::uint64_t resident_frame, graphics::buffer_handle skin_vertex_buffer = {}, graphics::buffer::address_type skin_vertex_address = 0u) -> void {
    _vertex_buffer = vertex_buffer;
    _index_buffer = index_buffer;
    _vertex_address = vertex_address;
    _skin_vertex_buffer = skin_vertex_buffer;
    _skin_vertex_address = skin_vertex_address;
    _resident_frame = resident_frame;
    _uploaded = true;
  }

  // Pure CPU data (no GPU upload wait involved), so asset_residency::load_mesh sets these directly,
  // before this mesh is even resident.
  auto _set_skeletal_data(skeleton_handle skeleton, std::vector<animation_clip_handle> animation_clips) -> void {
    _skeleton = std::move(skeleton);
    _animation_clips = std::move(animation_clips);
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
