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

#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/resources/buffer.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/types.hpp>

namespace sbx::graphics {

/**
 * @brief Stages CPU data into device-local images.
 */
class upload_context : public utility::noncopyable {

public:

  upload_context() = default;

  ~upload_context() = default;

  /**
   * @brief Queues @p pixels to be copied into @p destination (mip 0), ending in @p final_layout.
   *
   * @p destination must have `image_usage::transfer_destination`.
   *
   * @param destination The destination image.
   * @param pixels The pixel data to copy.
   * @param final_layout The layout the destination image should be transitioned to after the copy.
   */
  auto stage_image(image_handle destination, std::span<const std::byte> pixels, image_layout final_layout = image_layout::shader_read_only_optimal) -> void;

  /**
   * @brief Queues @p data to be copied into @p destination at @p destination_offset.
   *
   * @p destination must have `buffer_usage::transfer_destination`.
   *
   * @param destination The destination buffer.
   * @param data The data to copy.
   * @param destination_offset The offset into the destination buffer to copy to.
   */
  auto stage_buffer(buffer_handle destination, std::span<const std::byte> data, buffer::size_type destination_offset = 0u) -> void;

  /**
   * @brief Queues copying every mip of each image in @p layers into the array layer of
   * @p destination with the same position in the list, ending in @p final_layout.
   *
   * The caller checks the shapes: every layer has @p destination's extent, format and mip count,
   * and there are exactly as many layers as @p destination has. Each layer must be resident
   * (uploaded, in shader_read_only_optimal) and stay alive until the flush; it's back in
   * shader_read_only_optimal afterwards. @p destination needs `image_usage::transfer_destination`,
   * the layers `image_usage::transfer_source`.
   */
  auto stage_layer_copies(image_handle destination, std::vector<image_handle> layers, image_layout final_layout = image_layout::shader_read_only_optimal) -> void;

  /**
   * @brief Records every queued copy and its barriers into @p commands, retiring the staging
   * buffers at @p frame_index. Clears the queue.
   */
  auto flush(command_buffer& commands, std::uint64_t frame_index) -> void;

  [[nodiscard]] auto has_pending() const noexcept -> bool {
    return !_pending_images.empty() || !_pending_layer_copies.empty();
  }

private:

  struct pending_image {
    image_handle destination;
    buffer_handle staging;
    math::vector3u extent;
    std::uint32_t mip_levels;
    std::uint32_t array_layers;
    VkImageAspectFlags aspect;
    image_layout final_layout;
  }; // struct pending_image

  struct pending_buffer {
    buffer_handle destination;
    buffer_handle staging;
    buffer::size_type size;
    buffer::size_type destination_offset;
  }; // struct pending_buffer

  struct pending_layer_copy {
    image_handle destination;
    std::vector<image_handle> layers;
    image_layout final_layout;
  }; // struct pending_layer_copy

  std::vector<pending_image> _pending_images{};
  std::vector<pending_layer_copy> _pending_layer_copies{};
  std::vector<pending_buffer> _pending_buffers{};

}; // class upload_context

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_UPLOAD_CONTEXT_HPP_
