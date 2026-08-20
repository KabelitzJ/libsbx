// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_PIPELINE_BINARY_CACHE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_PIPELINE_BINARY_CACHE_HPP_

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

namespace sbx::graphics {

class logical_device;

/**
 * @brief The engine-wide `VkPipelineCache`, seeded from (and persisted back to) the active
 * project's `pipeline_cache.bin` on disk.
 *
 * Built eagerly, at construction — a project is guaranteed to be active by the time any
 * module constructs (see `core::engine_config::project`), so there is no "no project yet"
 * case to handle here.
 */
class pipeline_binary_cache : public utility::noncopyable {

public:

  using handle_type = VkPipelineCache;

  explicit pipeline_binary_cache(const graphics::logical_device& logical_device);

  ~pipeline_binary_cache();

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

private:

  handle_type _handle{VK_NULL_HANDLE};

}; // class pipeline_binary_cache

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_PIPELINE_BINARY_CACHE_HPP_
