// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_TARGET_HPP_
#define LIBSBX_UTILITY_TARGET_HPP_

#include <cstdint>
#include <type_traits>

namespace sbx::utility {

/** @brief Possible build configurations */
enum class build_type : std::uint8_t {
  debug = 0,
  release = 1
}; // enum class build_type

#if defined(SBX_BUILD_TYPE_DEBUG)
inline constexpr auto build_type_v = build_type::debug;
#else
inline constexpr auto build_type_v = build_type::release;
#endif

struct is_build_type_debug {
  inline static constexpr auto value = (build_type_v == build_type::debug);
}; // struct is_build_type_debug

inline constexpr auto is_build_type_debug_v = is_build_type_debug::value;

/** @brief Possible operating systems */
enum class operating_system : std::uint8_t {
  windows = 0,
  apple = 1,
  linux = 2,
  unknown = 3
}; // enum class operating_system

#if defined(SBX_PLATFORM_WIN32)
inline constexpr auto operating_system_v = operating_system::windows;
#elif defined(SBX_PLATFORM_APPLE)
inline constexpr auto operating_system_v = operating_system::apple;
#elif defined(SBX_PLATFORM_LINUX)
inline constexpr auto operating_system_v = operating_system::linux;
#else 
inline constexpr auto operating_system_v = operating_system::unknown;
#warning "Unknown operating system"
#endif

/** @brief Possible compilers */
enum class compiler : std::uint8_t {
  clang = 0,
  gnu = 1,
  msc = 2,
  unknown = 3
}; // enum class compiler

#if defined(SBX_COMPILER_CLANG)
inline constexpr auto compiler_v = compiler::clang;
#elif defined(SBX_COMPILER_GNU)
inline constexpr auto compiler_v = compiler::gnu;
#elif defined(SBX_COMPILER_MSC)
inline constexpr auto compiler_v = compiler::msc;
#else
inline constexpr auto compiler_v = compiler::unknown;
#endif

#if defined(SBX_CONSTEXPR_ENABLED)
#define SBX_CONSTEXPR constexpr
#else
#define SBX_CONSTEXPR
#endif

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_TARGET_HPP_
