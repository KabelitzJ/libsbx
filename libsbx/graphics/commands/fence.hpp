// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_COMMANDS_FENCE_HPP_
#define LIBSBX_GRAPHICS_COMMANDS_FENCE_HPP_

#include <string>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

namespace sbx::graphics {

/** @brief Owning VkFence, created on graphics_module's logical device. */
class fence : public utility::noncopyable {

public:

  using handle_type = VkFence;

  explicit fence(bool is_signaled = false, const std::string& name = {});

  fence(fence&& other) noexcept;

  ~fence();

  auto operator=(fence&& other) noexcept -> fence&;

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

  /** @brief Whether the GPU has signaled it. Never blocks. */
  [[nodiscard]] auto is_signaled() const -> bool;

  /** @brief Blocks until the GPU has signaled it. */
  auto wait() const -> void;

  auto reset() -> void;

private:

  auto _destroy() noexcept -> void;

  handle_type _handle{};

}; // class fence

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_COMMANDS_FENCE_HPP_
