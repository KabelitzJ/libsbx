// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_MANAGED_PLATFORM_HPP_
#define LIBSBX_SCRIPTING_MANAGED_PLATFORM_HPP_

#include <string_view>

#include <libsbx/utility/target.hpp>

namespace sbx::scripting::managed {

#ifdef SBX_WINDOWS
#define SBX_SCRIPTING_CALLTYPE __cdecl
#define SBX_SCRIPTING_HOSTFXR_NAME "hostfxr.dll"

#ifdef _WCHAR_T_DEFINED
#define SBX_SCRIPTING_STR(s) L##s
#define SBX_SCRIPTING_WIDE_CHARS

using char_type = wchar_t;

#else
#define SBX_SCRIPTING_STR(s) s

using char_type = unsigned short;
#endif
#else
#define SBX_SCRIPTING_CALLTYPE
#define SBX_SCRIPTING_STR(s) s
#define SBX_SCRIPTING_HOSTFXR_NAME "libhostfxr.so"

using char_type = char;
#endif

#define SBX_SCRIPTING_DOTNET_TARGET_VERSION_MAJOR 9
#define SBX_SCRIPTING_DOTNET_TARGET_VERSION_MAJOR_STR '9'
#define SBX_SCRIPTING_UNMANAGED_CALLERS_ONLY (reinterpret_cast<const char_type*>(-1))

using string_type = std::basic_string<char_type>;
using string_view_type = std::basic_string_view<char_type>;

} // namespace sbx::scripting::managed

#endif // LIBSBX_SCRIPTING_MANAGED_PLATFORM_HPP_
