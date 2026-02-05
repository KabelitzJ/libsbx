// SPDX-License-Identifier: MIT
#include <libsbx/graphics/draw_list.hpp>

namespace sbx::graphics {

template<typename Type>
auto draw_list::update_buffer(const memory::vector<Type>& buffer, const utility::hashed_string& name) -> void {
  auto& storage_buffer = get_buffer(name);

  const auto required_size = static_cast<std::uint32_t>(buffer.size() * sizeof(Type));

  if (storage_buffer.size() < required_size) {
    storage_buffer.resize(static_cast<std::size_t>(static_cast<std::float_t>(required_size) * 1.5f));
  }

  storage_buffer.update(buffer.data(), required_size);
}

} // namespace sbx::graphics
