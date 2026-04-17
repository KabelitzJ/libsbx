// SPDX-License-Identifier: MIT
#include <libsbx/graphics/buffers/storage_handler.hpp>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>

namespace sbx::graphics {

storage_handler::storage_handler(VkBufferUsageFlags additional_usage, const std::optional<shader::uniform_block>& uniform_block)
: _uniform_block{uniform_block},
  _additional_usage{additional_usage} {
  if (_uniform_block) {
    _storage_buffer = std::make_unique<graphics::storage_buffer>(graphics::storage_buffer::min_size, _additional_usage);
  }
}

auto storage_handler::storage_buffer() const noexcept -> const graphics::storage_buffer& {
  return *_storage_buffer;
}

auto storage_handler::update(const std::optional<shader::uniform_block>& uniform_block) -> bool {
  if (_uniform_block != uniform_block) {
    _uniform_block = uniform_block;
    const auto size = _storage_buffer ? _storage_buffer->size() : graphics::storage_buffer::min_size;
    _storage_buffer = std::make_unique<graphics::storage_buffer>(size, _additional_usage);

    return false; 
  }

  return true;
}

auto storage_handler::buffer_address() const noexcept -> std::uint64_t {
  if (!_storage_buffer) {
    utility::logger<"storage_handler">::warn("Trying to get address of storage_handler that has no storage_buffer");
  }

  return _storage_buffer ? _storage_buffer->address() : 0u;
}

} // namespace sbx::graphics
