// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ENGINE_HPP_
#define LIBSBX_ENGINE_HPP_

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/platform_module.hpp>

namespace sbx {

/**
 * @brief The default module composition of libsbx.
 *
 * Applications that need a different set (headless tools, tests) compose
 * core::basic_engine themselves.
 */
using engine = core::basic_engine<
  platform::platform_module
>;

} // namespace sbx

#endif // LIBSBX_ENGINE_HPP_
