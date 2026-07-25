// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_MESH_HPP_
#define LIBSBX_ASSETS_MESH_HPP_

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
 * @brief A loaded mesh: one device-local vertex + index buffer, drawn as one or more submeshes
 * (one per glTF primitive). The GPU buffers are filled on the render thread; the mesh is drawable
 * once resident. The vertex buffer is read in the shader via its device address (BDA).
 */
class mesh final {

  friend class assets_module;

public:

  struct submesh {
    std::uint32_t index_offset;
    std::uint32_t index_count;
    math::volume bounds;
    material_handle material;
  }; // struct submesh

  mesh() = default;

  mesh(std::vector<submesh> submeshes, const math::volume& bounds)
  : _submeshes{std::move(submeshes)}, _bounds{bounds} { }

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

  [[nodiscard]] auto index_buffer() const noexcept -> const graphics::buffer_handle& {
    return _index_buffer;
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

  // Called once by assets_module on the render thread, after the GPU buffers exist.
  auto _finalize(graphics::buffer_handle vertex_buffer, graphics::buffer_handle index_buffer, graphics::buffer::address_type vertex_address, std::uint64_t resident_frame) -> void {
    _vertex_buffer = vertex_buffer;
    _index_buffer = index_buffer;
    _vertex_address = vertex_address;
    _resident_frame = resident_frame;
    _uploaded = true;
  }

  std::vector<submesh> _submeshes{};
  graphics::buffer_handle _vertex_buffer{};
  graphics::buffer_handle _index_buffer{};
  graphics::buffer::address_type _vertex_address{0u};
  std::uint64_t _resident_frame{0u};
  bool _uploaded{false};
  math::volume _bounds{};
  math::uuid _id{math::uuid::nil()};

}; // class mesh

using mesh_handle = asset_handle<mesh>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_MESH_HPP_
