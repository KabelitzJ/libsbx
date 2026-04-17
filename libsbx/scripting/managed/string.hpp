// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_MANAGED_STRING_HPP_
#define LIBSBX_SCRIPTING_MANAGED_STRING_HPP_

#include <string>

#include <libsbx/scripting/managed/core.hpp>

namespace sbx::scripting::managed {

class string {

public:

  string();

  static auto create(const char* str) -> string;
  static auto create(std::string_view str) -> string;
  static auto destroy(string& str) -> void;

  auto assign(std::string_view str) -> void;

  operator std::string() const;

  auto operator==(const string& other) const -> bool;
  auto operator==(std::string_view other) const -> bool;

  auto data() -> char_type*;
  auto data() const -> const char_type*;

private:

  char_type* _string;
  bool32 _is_disposed;

}; // class string

class string_helper {

public:

  static auto convert_utf8_to_wide(std::string_view str) -> string_type;
  static auto convert_wide_to_utf8(string_view_type str) -> std::string;

}; // class string_helper

} // namespace sbx::scripting::managed  

#endif // LIBSBX_SCRIPTING_MANAGED_STRING_HPP_