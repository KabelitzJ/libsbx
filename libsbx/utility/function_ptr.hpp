// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_FUNCTION_PTR_HPP_
#define LIBSBX_UTILITY_FUNCTION_PTR_HPP_

namespace sbx::utility {

/**
 * @brief Alias for a plain (non-capturing) function pointer type.
 *
 * @tparam Return The return type.
 * @tparam Args The parameter types.
 */
template<typename Return, typename... Args>
using function_ptr = Return(*)(Args...);

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_FUNCTION_PTR_HPP_
