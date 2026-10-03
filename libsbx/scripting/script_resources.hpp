// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCRIPTING_SCRIPT_RESOURCES_HPP_
#define LIBSBX_SCRIPTING_SCRIPT_RESOURCES_HPP_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/math/volume.hpp>

#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/commands/fence.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

#include <libsbx/assets/mesh.hpp>

#include <libsbx/scenes/instance_buffer.hpp>

namespace sbx::scripting {

/** @brief A SetGeometry call waiting for interop::apply_pending_geometry; a later call for the same node replaces it. */
struct pending_geometry {
  std::vector<assets::vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<assets::mesh::submesh> submeshes;
  math::volume bounds;
  bool instanced{false};  // for the node's instanced_mesh_renderer, not its mesh_renderer
}; // struct pending_geometry

struct compute_buffer_state {
  graphics::buffer_handle handle;
  std::uint32_t stride;
}; // struct compute_buffer_state

struct compute_shader_state {
  std::filesystem::path path;
  memory::observer_ptr<graphics::compute_pipeline> pipeline;
  graphics::shader_compiler::push_constant_layout layout;
  std::vector<std::byte> params;
  std::vector<bool> is_set;
}; // struct compute_shader_state

struct compute_commands_state {
  graphics::command_buffer command_buffer;
  std::uint32_t dispatch_count{0u};
  std::optional<graphics::fence> fence{}; // set once submitted without waiting
}; // struct compute_commands_state

/**
 * @brief Everything scripts create through interop that outlives a single call, keyed by the id handed to managed code.
 *
 * Owned by scripting_module, so it is released while the graphics module still exists, and cleared
 * whenever the scripts that could reference it go away (play mode stop, script reload).
 */
struct script_resources : public utility::noncopyable {

  script_resources() = default;

  ~script_resources();

  /** @brief Waits for in-flight compute work, retires every compute buffer and drops all pending geometry. */
  auto clear() -> void;

  std::unordered_map<std::uint64_t, pending_geometry> pending_geometries;
  std::unordered_map<std::uint64_t, std::vector<scenes::instance_data>> pending_instances;

  std::unordered_map<std::uint64_t, compute_buffer_state> compute_buffers;
  std::unordered_map<std::uint64_t, compute_shader_state> compute_shaders;
  std::unordered_map<std::uint64_t, compute_commands_state> compute_commands;

}; // struct script_resources

} // namespace sbx::scripting

#endif // LIBSBX_SCRIPTING_SCRIPT_RESOURCES_HPP_
