// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CORE_EXIT_HPP_
#define LIBSBX_CORE_EXIT_HPP_

#include <cinttypes>

namespace sbx::core {

struct exit {
  inline static constexpr auto success = std::int32_t{0};
  inline static constexpr auto failure = std::int32_t{1};
}; // struct exit

} // namespace sbx::core

#endif // LIBSBX_CORE_EXIT_HPP_
