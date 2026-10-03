// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_INSTANCE_BUFFER_HPP_
#define LIBSBX_SCENES_INSTANCE_BUFFER_HPP_

#include <cstdint>
#include <span>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/graphics/resources/buffer.hpp>

namespace sbx::scenes {

/**
 * @brief One instance of an instanced_mesh_renderer, as the GPU reads it (80 bytes): an affine
 * transform relative to the renderer's node, as the top three rows of a 4x4 matrix (row i =
 * (m_i0, m_i1, m_i2, translation_i)), plus two values the material's shader reads per instance --
 * `color` (multiplied into the vertex colour by the engine's shaders; white = unchanged) and
 * `custom_data` (anything the shader wants; the engine ignores it). Godot's MultiMesh
 * instance_color / instance_custom_data.
 */
struct instance_data {
  math::vector4 rows[3];
  math::color color{math::color::white()};
  math::vector4 custom_data{0.0f, 0.0f, 0.0f, 0.0f};
}; // struct instance_data

static_assert(sizeof(instance_data) == 80u, "instance_data must match frustum_cull_instanced.slang's layout");

/**
 * @brief A GPU buffer of instance_data, written once on creation and never changed -- replace the
 * whole instance_buffer to change the instances.
 *
 * Create only while the render thread is idle (presentation_module::on_render_idle): creating
 * writes the resource registry. Destruction is safe anywhere: it only queues the buffer with
 * scenes_module, whose collect_released_instance_buffers (on render idle) retires it.
 */
class instance_buffer : public utility::noncopyable {

public:

  explicit instance_buffer(std::span<const instance_data> instances);

  ~instance_buffer();

  /** @brief The GPU buffer; render thread only (resource registry lookups race its writes elsewhere). */
  [[nodiscard]] auto buffer() const noexcept -> graphics::buffer_handle {
    return _buffer;
  }

  [[nodiscard]] auto count() const noexcept -> std::uint32_t {
    return _count;
  }

private:

  graphics::buffer_handle _buffer{};
  std::uint32_t _count{0u};

}; // class instance_buffer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_INSTANCE_BUFFER_HPP_
