// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/utility/base64.hpp>

#include <array>
#include <cstdint>

namespace sbx::utility {

inline constexpr auto base64_alphabet = std::string_view{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

// 0xFF marks a character outside the alphabet.
inline constexpr auto base64_values = [] {
  auto values = std::array<std::uint8_t, 256u>{};
  values.fill(0xFFu);

  for (auto index = std::size_t{0u}; index < base64_alphabet.size(); ++index) {
    values[static_cast<std::uint8_t>(base64_alphabet[index])] = static_cast<std::uint8_t>(index);
  }

  return values;
}();

auto base64_encode(std::span<const char> bytes) -> std::string {
  auto text = std::string{};
  text.reserve((bytes.size() + 2u) / 3u * 4u);

  for (auto index = std::size_t{0u}; index < bytes.size(); index += 3u) {
    const auto remaining = bytes.size() - index;

    const auto b0 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[index]));
    const auto b1 = remaining > 1u ? static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[index + 1u])) : 0u;
    const auto b2 = remaining > 2u ? static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[index + 2u])) : 0u;

    const auto triple = (b0 << 16u) | (b1 << 8u) | b2;

    text.push_back(base64_alphabet[(triple >> 18u) & 0x3Fu]);
    text.push_back(base64_alphabet[(triple >> 12u) & 0x3Fu]);
    text.push_back(remaining > 1u ? base64_alphabet[(triple >> 6u) & 0x3Fu] : '=');
    text.push_back(remaining > 2u ? base64_alphabet[triple & 0x3Fu] : '=');
  }

  return text;
}

auto base64_decode(std::string_view text) -> std::optional<std::vector<char>> {
  if (text.size() % 4u != 0u) {
    return std::nullopt;
  }

  auto bytes = std::vector<char>{};
  bytes.reserve(text.size() / 4u * 3u);

  for (auto index = std::size_t{0u}; index < text.size(); index += 4u) {
    const auto is_last = index + 4u == text.size();
    const auto padding = (is_last && text[index + 3u] == '=') ? (text[index + 2u] == '=' ? 2u : 1u) : 0u;

    auto triple = std::uint32_t{0u};

    for (auto offset = std::size_t{0u}; offset < 4u; ++offset) {
      const auto character = text[index + offset];

      if (offset >= 4u - padding) {
        triple <<= 6u;
        continue;
      }

      const auto value = base64_values[static_cast<std::uint8_t>(character)];

      if (value == 0xFFu) {
        return std::nullopt;
      }

      triple = (triple << 6u) | value;
    }

    bytes.push_back(static_cast<char>((triple >> 16u) & 0xFFu));

    if (padding < 2u) {
      bytes.push_back(static_cast<char>((triple >> 8u) & 0xFFu));
    }

    if (padding < 1u) {
      bytes.push_back(static_cast<char>(triple & 0xFFu));
    }
  }

  return bytes;
}

} // namespace sbx::utility
