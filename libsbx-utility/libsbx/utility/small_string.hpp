// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_SMALL_STRING_HPP_
#define LIBSBX_UTILITY_SMALL_STRING_HPP_

#include <array>
#include <functional>
#include <iterator>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <cstring>

#include <libsbx/utility/hash.hpp>

namespace sbx::utility {

template<character Char, typename Traits = std::char_traits<Char>>
class small_string {

  using traits_type = Traits;

public:

  using char_type = Char;
  using size_type = std::size_t;

  constexpr small_string() {
    std::memset(_buffer.stack, 0, sizeof(buffer));
  }

private:

  union buffer {
    struct heap {
      char_type* data;
      size_type size;
    } heap;
    char_type stack[sizeof(heap)];
  } _buffer;

}; // class small_string

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_SMALL_STRING_HPP_
