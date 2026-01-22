// SPDX-License-Identifier: MIT
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
 * @file libsbx/utility/utility.hpp 
 */

#ifndef LIBSBX_UTILITY_HPP_
#define LIBSBX_UTILITY_HPP_

/**
 * @defgroup libsbx-utility
 * 
 * @brief The core library of the libsbx project.
 * @version v0.1.0
 * @since v0.1.0
 * 
 * @author Jonas Kabelitz <jonas-kabelitz@gmx.de>
 * @date 2023-02-21
 */

#include <libsbx/utility/version.hpp>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/bitmask.hpp>
#include <libsbx/utility/compression.hpp>
#include <libsbx/utility/concepts.hpp>
#include <libsbx/utility/enable_private_constructor.hpp>
#include <libsbx/utility/enum.hpp>
#include <libsbx/utility/expected.hpp>
#include <libsbx/utility/fast_mod.hpp>
#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/layout.hpp>
#include <libsbx/utility/make_array.hpp>
#include <libsbx/utility/multimap_key_range.hpp>
#include <libsbx/utility/overload.hpp>
#include <libsbx/utility/primitive.hpp>
#include <libsbx/utility/small_string.hpp>
#include <libsbx/utility/string_literal.hpp>
#include <libsbx/utility/target.hpp>
#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/type_name.hpp>
#include <libsbx/utility/zip.hpp>

#endif // LIBSBX_UTILITY_HPP_
