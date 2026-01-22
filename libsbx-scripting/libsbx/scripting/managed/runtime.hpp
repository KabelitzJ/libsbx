// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_MANAGED_RUNTIME_HPP_
#define LIBSBX_SCRIPTING_MANAGED_RUNTIME_HPP_

#include <string>
#include <functional>
#include <filesystem>

#include <libsbx/scripting/managed/core.hpp>
#include <libsbx/scripting/managed/object.hpp>
#include <libsbx/scripting/managed/message_type.hpp>
#include <libsbx/scripting/managed/assembly.hpp>

namespace sbx::scripting::managed {

using message_callback_fn = std::function<void(std::string_view, message_level)>;
using exception_callback_fn = std::function<void(std::string_view)>;

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

}; // class runtime

}; // namespace sbx::scripting::managed

#endif // LIBSBX_SCRIPTING_MANAGED_RUNTIME_HPP_
