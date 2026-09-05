// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_SHADER_CACHE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_SHADER_CACHE_HPP_

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::graphics {

/**
 * @brief Owns every compiled shader for the run, deduplicated by source path and requested entry points.
 *
 * Each distinct shader gets a stable @ref shader::id_type that pipelines key on. Not thread-safe — compilation is a render-thread operation, like the resource pool.
 */
class shader_cache : public utility::noncopyable {

public:

  struct request {
    std::filesystem::path path;
    std::vector<shader_compiler::entry_point_request> entry_points;
  }; // struct request

  explicit shader_cache();

  ~shader_cache() = default;

  [[nodiscard]] auto get(const request& request) -> memory::observer_ptr<const shader>;

private:

  struct key {
    std::string path;
    std::vector<shader_compiler::entry_point_request> entry_points;

    auto operator==(const key&) const -> bool = default;
  }; // struct key

  struct key_hash {
    auto operator()(const key& key) const noexcept -> std::size_t;
  }; // struct key_hash

  std::unordered_map<key, std::unique_ptr<shader>, key_hash> _shaders{};
  shader::id_type _next_id{1u};

}; // class shader_cache

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_SHADER_CACHE_HPP_
