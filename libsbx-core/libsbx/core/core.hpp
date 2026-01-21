/* 
 * Copyright (c) 2022 Jonas Kabelitz
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * You should have received a copy of the MIT License along with this program.
 * If not, see <https://opensource.org/licenses/MIT/>.
 */

/**
 * @file libsbx/core/core.hpp 
 */

#ifndef LIBSBX_CORE_HPP_
#define LIBSBX_CORE_HPP_

/**
 * @defgroup libsbx-core
 * 
 * @brief The core library of the libsbx project.
 * @version v0.1.0
 * @since v0.1.0
 * 
 * @author Jonas Kabelitz <jonas-kabelitz@gmx.de>
 * @date 2023-02-21
 * 
 * External dependencies:
 *   - <a href="https://cppget.org/spdlog">spdlog</a>
 *   - <a href="https://cppget.org/gtest">gtest</a>
 * 
 * Internal dependencies:
 *   - @ref libsbx-utility
 */

#include <libsbx/core/version.hpp>

#include <libsbx/core/application.hpp>
#include <libsbx/core/concepts.hpp>
#include <libsbx/core/engine.hpp>
#include <libsbx/core/module.hpp>
#include <libsbx/core/delegate.hpp>
#include <libsbx/core/exit.hpp>
#include <libsbx/core/cli.hpp>

#endif // LIBSBX_CORE_HPP_
