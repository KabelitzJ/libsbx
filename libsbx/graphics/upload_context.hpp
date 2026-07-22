// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_UPLOAD_CONTEXT_HPP_
#define LIBSBX_GRAPHICS_UPLOAD_CONTEXT_HPP_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/vector3.hpp>

#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/resources/buffer.hpp>

namespace sbx::graphics {

class command_buffer;

/**
 * @brief Stages CPU data into device-local images.
 */
class upload_context : public utility::noncopyable {

public:

  upload_context() = default;

  ~upload_context() = default;

  /**
   * @brief Queues @p pixels to be copied into @p destination (mip 0), ending in
   * @p final_layout. The destination must have `image_usage::transfer_destination`.
   */
  auto stage_image(image_handle destination, std::span<const std::byte> pixels, image_layout final_layout = image_layout::shader_read_only_optimal) -> void;

  /**
   * @brief Records every queued copy and its barriers into @p commands, retiring the staging
   * buffers at @p frame_index. Clears the queue.
   */
  auto flush(command_buffer& commands, std::uint64_t frame_index) -> void;

  [[nodiscard]] auto has_pending() const noexcept -> bool {
    return !_pending_images.empty();
  }

private:

  struct pending_image {
    image_handle destination;
    buffer_handle staging;
    math::vector3u extent;
    std::uint32_t array_layers;
    VkImageAspectFlags aspect;
    image_layout final_layout;
  }; // struct pending_image

  std::vector<pending_image> _pending_images{};

}; // class upload_context

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_UPLOAD_CONTEXT_HPP_
