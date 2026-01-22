// SPDX-License-Identifier: MIT
#include <libsbx/scripting/managed/string.hpp>

#include <cstring>

#if defined(SBX_SCRIPTING_WIDE_CHARS)
#include <codecvt>
#endif

#include <libsbx/utility/target.hpp>

#include <libsbx/scripting/managed/memory.hpp>

#if defined(SBX_WINDOWS)
#include <windows.h>
#include <stringapiset.h>
#endif

namespace sbx::scripting::managed {

string::string()
: _string{nullptr},
  _is_disposed{false} { }

auto string::create(const char* str) -> string {
  auto result = string{};

  result.assign(str);

  return result;
}

auto string::create(std::string_view str) -> string {
  auto result = string{};

  result.assign(str);
  
  return result;
}

auto string::destroy(string& str) -> void {
  if (str._string != nullptr) {
    memory::free_co_task_memory(str._string);
    str._string = nullptr;
  }
}

auto string::assign(std::string_view str) -> void {
  if (_string != nullptr) {
    memory::free_co_task_memory(_string);
  }

  _string = memory::string_to_co_task_memory_auto(string_helper::convert_utf8_to_wide(str));
}

string::operator std::string() const
{
  if (!_string) {
    return std::string{"[NullString]"};
  }

  auto string = string_view_type{_string};

#if defined(SBX_SCRIPTING_WIDE_CHARS)
  return string_helper::convert_wide_to_utf8(string);
#else
  return std::string{string};
#endif
}

auto string::operator==(const string& other) const -> bool {
  if (_string == other._string) {
    return true;
  }

  if (_string == nullptr || other._string == nullptr) {
    return false;
  }

#if defined(SBX_SCRIPTING_WIDE_CHARS)
  return wcscmp(_string, other._string) == 0;
#else
  return strcmp(_string, other._string) == 0;
#endif
}

auto string::operator==(std::string_view other) const -> bool {
  if (_string == nullptr && other.empty()) {
    return true;
  }

  if (_string == nullptr) {
    return false;
  }

#if defined(SBX_SCRIPTING_WIDE_CHARS)
  auto str = string_helper::convert_utf8_to_wide(other);

  return wcscmp(_string, str.data()) == 0;
#else
  return strcmp(_string, other.data()) == 0;
#endif
}

auto string::data() -> char_type* {
  return _string;
}

auto string::data() const -> const char_type* {
  return _string;
}

auto string_helper::convert_utf8_to_wide(std::string_view str) -> string_type {
#if defined(SBX_SCRIPTING_WIDE_CHARS)
  auto length = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr, 0);
  auto result = std::wstring(length, wchar_t(0));

  MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), result.data(), length);

  return result;
#else
  return std::string{str};
#endif
}

auto string_helper::convert_wide_to_utf8(string_view_type str) -> std::string {
#if defined(SBX_SCRIPTING_WIDE_CHARS)
  auto required_length = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr, 0, nullptr, nullptr);
  auto result = std::string(required_length, 0);

  (void)WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), result.data(), required_length, nullptr, nullptr);

  return result;
#else
  return std::string{str};
#endif
}

} // namespace sbx::scripting::managed
