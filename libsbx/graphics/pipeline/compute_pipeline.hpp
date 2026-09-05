// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_COMPUTE_PIPELINE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_COMPUTE_PIPELINE_HPP_

#include <string>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>

namespace sbx::graphics {

/**
 * @brief A compute pipeline built against the shared bindless pipeline layout, with a single VK_SHADER_STAGE_COMPUTE_BIT stage and no rasterizer state.
 *
 * Owned by @ref compute_pipeline_cache; construct via the cache, not directly.
 */
class compute_pipeline : public utility::noncopyable {

public:

  using handle_type = VkPipeline;

  struct create_info {
    memory::observer_ptr<const graphics::shader> shader{};
    std::string name{"Compute Pipeline"};
  }; // struct create_info

  explicit compute_pipeline(const create_info& create_info);

  ~compute_pipeline();

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

  [[nodiscard]] auto bind_point() const noexcept -> pipeline_bind_point {
    return pipeline_bind_point::compute;
  }

private:

  handle_type _handle{};

}; // class compute_pipeline

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_COMPUTE_PIPELINE_HPP_
