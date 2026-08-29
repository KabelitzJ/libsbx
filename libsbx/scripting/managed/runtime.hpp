// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCRIPTING_MANAGED_RUNTIME_HPP_
#define LIBSBX_SCRIPTING_MANAGED_RUNTIME_HPP_

#include <string>
#include <functional>
#include <filesystem>
#include <span>
#include <vector>

#include <libsbx/scripting/managed/core.hpp>
#include <libsbx/scripting/managed/object.hpp>
#include <libsbx/scripting/managed/message_type.hpp>
#include <libsbx/scripting/managed/assembly.hpp>

namespace sbx::scripting::managed {

using message_callback_fn = std::function<void(std::string_view, message_level)>;
using exception_callback_fn = std::function<void(std::string_view)>;

/** @brief One diagnostic from a compile_scripts() call — a Roslyn error/warning. */
struct compiler_diagnostic {
  bool is_error;
  std::string file;
  std::int32_t line;
  std::int32_t column;
  std::string message;
}; // struct compiler_diagnostic

struct compile_result {
  bool success;
  std::vector<compiler_diagnostic> diagnostics;
}; // struct compile_result

struct rumtime_config {
  std::string backend_path;

  message_callback_fn message_callback;
  message_level message_filter = message_level::all;

  exception_callback_fn exception_callback;
}; // struct rumtime_config

enum class runtime_status {
  success,
  managed_not_found,
  managed_init_error,
  dot_net_not_found
}; // enum class runtime_status

class runtime {

  friend class assembly_load_context;

public:

  auto initialize(rumtime_config settings) -> runtime_status;

  auto shutdown() -> void;

  auto create_assembly_load_context(std::string_view name) -> assembly_load_context;

  auto unload_assembly_load_context(assembly_load_context& load_context) -> void;

  /**
   * @brief Compiles @p source_paths into a DLL at @p output_path via Sbx.Compiler (in-process
   * Roslyn — see Sbx.Compiler/Compiler.cs), referencing @p reference_paths plus whatever the
   * installed .NET shared framework provides. Resolved lazily on first call, from the same
   * backend_path directory Sbx.Managed.dll was loaded from — see Sbx.Compiler's own CMake publish
   * target, which places it alongside Sbx.Managed.dll/Sbx.Core.dll.
   */
  auto compile_scripts(std::span<const std::string> source_paths, std::span<const std::string> reference_paths, const std::filesystem::path& output_path) -> compile_result;

private:

  auto load_host_fxr() const -> bool;

  auto initialize_managed() -> bool;

  auto load_functions() -> void;

  auto load_managed_function_ptr(const std::filesystem::path& assembly_path, const char_type* type_name, const char_type* method_name, const char_type* delegate_type = SBX_SCRIPTING_UNMANAGED_CALLERS_ONLY) const -> void*;

  template<typename Function>
  auto load_managed_function_ptr(const char_type* type_name, const char_type* method_name, const char_type* delegate_type = SBX_SCRIPTING_UNMANAGED_CALLERS_ONLY) const -> Function {
    return reinterpret_cast<Function>(load_managed_function_ptr(_managed_assembly_path, type_name, method_name, delegate_type));
  }

private:

  rumtime_config _settings;
  std::filesystem::path _managed_assembly_path;
  void* _host_fxr_context = nullptr;
  bool _initialized = false;

  // Sbx.Compiler.dll's Compile entry point — a separate component from Sbx.Managed (see
  // compile_scripts's doc comment), resolved lazily since nothing needs it until a script is
  // actually compiled.
  void* _compile_scripts_fn = nullptr;

}; // class runtime

}; // namespace sbx::scripting::managed

#endif // LIBSBX_SCRIPTING_MANAGED_RUNTIME_HPP_
