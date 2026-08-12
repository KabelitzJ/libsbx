// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_COMPUTE_PIPELINE_CACHE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_COMPUTE_PIPELINE_CACHE_HPP_

#include <memory>
#include <unordered_map>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

namespace sbx::graphics {

/**
 * @brief Owns every compute pipeline for the run, deduplicated by shader id — a compute pipeline
 * has no rasterizer state to key on, unlike @ref pipeline_cache.
 *
 * Not thread-safe, same as @ref pipeline_cache — currently fine because compute pipelines are
 * only ever created by whichever single thread drives the environment-map IBL bake at load time.
 * If a render-thread compute pass (skinning, GPU culling, particles) starts using this too, it
 * will need a mutex, the same way resource_registry got one.
 */
class compute_pipeline_cache : public utility::noncopyable {

public:

  compute_pipeline_cache() = default;

  ~compute_pipeline_cache() = default;

  [[nodiscard]] auto get(const compute_pipeline::create_info& create_info) -> memory::observer_ptr<compute_pipeline>;

private:

  std::unordered_map<graphics::shader::id_type, std::unique_ptr<compute_pipeline>> _pipelines{};

}; // class compute_pipeline_cache

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_COMPUTE_PIPELINE_CACHE_HPP_
