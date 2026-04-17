// SPDX-License-Identifier: MIT
#include <libsbx/graphics/buffers/uniform_handler.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/render_pass/swapchain.hpp>

namespace sbx::graphics {

uniform_handler::uniform_handler(const std::optional<shader::uniform_block>& uniform_block)
: _uniform_block{uniform_block} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (_uniform_block) {
    // _uniform_buffer = std::make_unique<graphics::uniform_buffer>(_uniform_block->size());
    _uniform_buffer = graphics_module.add_resource<graphics::uniform_buffer>(_uniform_block->size());
  }
}

auto uniform_handler::uniform_buffer() const noexcept -> const graphics::uniform_buffer& {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  
  return graphics_module.get_resource<graphics::uniform_buffer>(_uniform_buffer);
}

auto uniform_handler::update(const std::optional<shader::uniform_block>& uniform_block) -> bool {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (_uniform_block != uniform_block) {
    if (_uniform_buffer.is_valid()) {
      graphics_module.remove_resource<graphics::uniform_buffer>(_uniform_buffer);
    }

    _uniform_block = uniform_block;

    _uniform_buffer = graphics_module.add_resource<graphics::uniform_buffer>(_uniform_block->size());

    return false; 
  }

  return true;
}

auto uniform_handler::_push(std::span<const std::uint8_t> buffer, std::size_t offset) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    
  auto& uniform_buffer = graphics_module.get_resource<graphics::uniform_buffer>(_uniform_buffer);
  
  uniform_buffer.update(buffer.data(), buffer.size(), offset);
}

} // namespace sbx::graphics
