// SPDX-License-Identifier: MIT
#include <libsbx/graphics/buffers/uniform_handler.hpp>

#include <libsbx/core/engine.hpp>

namespace sbx::graphics {

template<typename Type>
auto uniform_handler::push(const Type& object, std::size_t size, std::size_t offset) -> void {
  if (!_uniform_block || !_uniform_buffer) {
    return;
  }

  _push({reinterpret_cast<const std::uint8_t*>(std::addressof(object)), size}, offset);
}

template<typename Type>
auto uniform_handler::push(const std::string& uniform_name, const Type& object, std::size_t size) -> void {
  if (!_uniform_block || !_uniform_buffer) {
    return;
  }

  auto uniform = _uniform_block->find_uniform(uniform_name);

  if (!uniform) {
    throw std::runtime_error{fmt::format("Uniform '{}' not found in uniform block", uniform_name)};
  }

  auto object_size = (size > 0) ? size : std::min(sizeof(Type), static_cast<std::size_t>(uniform->size()));

  push(object, object_size, static_cast<std::size_t>(uniform->offset()));
}

} // namespace sbx::graphics
