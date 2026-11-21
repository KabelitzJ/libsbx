// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_BASE64_HPP_
#define LIBSBX_UTILITY_BASE64_HPP_

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sbx::utility {

/**
 * @brief Encodes bytes as base64 (RFC 4648, standard alphabet, padded), for binary data inside text formats such as scene files.
 *
 * @param bytes The bytes.
 *
 * @return The encoded text.
 */
[[nodiscard]] auto base64_encode(std::span<const char> bytes) -> std::string;

/**
 * @brief Decodes base64 text written by base64_encode.
 *
 * @param text The encoded text; whitespace isn't allowed.
 *
 * @return The bytes, or nullopt if @p text isn't valid padded base64.
 */
[[nodiscard]] auto base64_decode(std::string_view text) -> std::optional<std::vector<char>>;

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_BASE64_HPP_
