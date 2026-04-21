// SPDX-License-Identifier: MIT
#include <libsbx/graphics/draw_list.hpp>

namespace sbx::graphics {

template<typename Type>
auto draw_list::update_buffer(const std::vector<Type>& buffer, const utility::hashed_string& name) -> void {
  auto& storage_buffer = get_buffer(name);

  const auto required_size = static_cast<VkDeviceSize>(buffer.size() * sizeof(Type));

  if (storage_buffer.size() < required_size) {
    storage_buffer.resize(required_size + required_size / 2);
  }

  storage_buffer.update(buffer.data(), required_size);
}

} // namespace sbx::graphics
