// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_COMMANDS_SEMAPHORE_HPP_
#define LIBSBX_GRAPHICS_COMMANDS_SEMAPHORE_HPP_

#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

namespace sbx::graphics {

/** @brief Owning VkSemaphore (binary or timeline, starting at 0), created on graphics_module's logical device. */
class semaphore : public utility::noncopyable {

public:

  using handle_type = VkSemaphore;

  enum class type : std::uint8_t {
    binary,
    timeline
  }; // enum class type

  explicit semaphore(type kind = type::binary, const std::string& name = {});

  semaphore(semaphore&& other) noexcept;

  ~semaphore();

  auto operator=(semaphore&& other) noexcept -> semaphore&;

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

  /** @brief Timeline only: the counter's current value. Never blocks. */
  [[nodiscard]] auto value() const -> std::uint64_t;

  /** @brief Timeline only: blocks until the counter reaches @p value. */
  auto wait(std::uint64_t value) const -> void;

private:

  auto _destroy() noexcept -> void;

  handle_type _handle{};

}; // class semaphore

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_COMMANDS_SEMAPHORE_HPP_
