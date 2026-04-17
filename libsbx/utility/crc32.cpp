// SPDX-License-Identifier: MIT
#include <libsbx/utility/crc32.hpp>

namespace sbx::utility {

static consteval auto make_crc32_table() -> std::array<std::uint32_t, 256> {
  auto table = std::array<std::uint32_t, 256>{};

  for (auto i = 0u; i < 256u; ++i) {
    auto c = i;

    for (auto j = 0; j < 8; ++j) {
      c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    }

    table[i] = c;
  }

  return table;
}

auto crc32(std::span<const std::uint8_t> buffer, std::uint32_t crc) -> std::uint32_t {
  static constexpr auto table = make_crc32_table();

  for (auto i = 0u; i < buffer.size(); ++i) {
    crc = table[(crc ^ buffer[i]) & 0xFFu] ^ (crc >> 8);
  }

  return crc ^ 0xFFFFFFFFu;
}

} // namespace sbx::utility