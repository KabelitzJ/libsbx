// SPDX-License-Identifier: MIT
#include <libsbx/scripting/managed/memory.hpp>

#include <cstdlib>
#include <cstring>

#include <memory>

#include <libsbx/utility/target.hpp>

#if defined(SBX_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#endif

namespace sbx::scripting::managed {

auto memory::allocate_hglobal(std::size_t size) -> void* {
#if defined(SBX_WINDOWS)
  return LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, size);
#else
  return malloc(size);
#endif
}

auto memory::free_hglobal(void* pointer) -> void {
#if defined(SBX_WINDOWS)
  LocalFree(pointer);
#else
  free(pointer);
#endif
}

auto memory::string_to_co_task_memory_auto(string_view_type string) -> char_type* {
  auto length = string.length() + 1;
  auto size = length * sizeof(char_type);

#if defined(SBX_WINDOWS)
  auto* buffer = static_cast<char_type*>(CoTaskMemAlloc(size));

  if (buffer != nullptr) {
    memset(buffer, 0xCE, size);
    wcscpy(buffer, string.data());
  }
#else
  auto* buffer = static_cast<char_type*>(allocate_hglobal(size));

  if (buffer != nullptr) {
    memset(buffer, 0, size);
    strcpy(buffer, string.data());
  }
#endif

  return buffer;
}

auto memory::free_co_task_memory(void* memory) -> void {
#if defined(SBX_WINDOWS)
  CoTaskMemFree(memory);
#else
  free_hglobal(memory);
#endif
}

} // namespace sbx::scripting::managed
