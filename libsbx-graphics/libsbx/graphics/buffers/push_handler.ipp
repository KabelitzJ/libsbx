// SPDX-License-Identifier: MIT
#include <libsbx/graphics/buffers/push_handler.hpp>

#include <fmt/format.h>

#include <type_traits>
#include <cstring>

namespace sbx::graphics {

template<typename Type>
auto push_handler::push(const Type& object, std::size_t size, std::size_t offset) -> void {
  std::memcpy(_data.get() + offset, std::addressof(object), size);
}

template<typename Type>
auto push_handler::push(const std::string& uniform_name, const Type& object, std::size_t size) -> void {
  if (!_uniform_block) {
    return;
  }

  auto uniform = _uniform_block->find_uniform(uniform_name);

  if (!uniform) {
    throw std::runtime_error{fmt::format("Uniform '{}' not found in push block", uniform_name)};
  }

  auto object_size = (size > 0) ? size : std::min(sizeof(Type), static_cast<std::size_t>(uniform->size()));

  push(object, object_size, static_cast<std::size_t>(uniform->offset()));
}

} // namespace sbx::graphics
