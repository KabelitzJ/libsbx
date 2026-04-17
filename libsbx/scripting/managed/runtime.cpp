// SPDX-License-Identifier: MIT
#include <libsbx/scripting/managed/runtime.hpp>

#include <iostream>

#include <libsbx/utility/target.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/scripting/managed/detail/backend.hpp>
#include <libsbx/scripting/managed/detail/type_cache.hpp>

#include <libsbx/scripting/managed/platform.hpp>

#if defined(SBX_WINDOWS)
	#include <ShlObj.h>
#else
	#include <dlfcn.h>
#endif

#include <hostfxr.h>
#include <nethost.h>
#include <coreclr_delegates.h>

namespace sbx::scripting::managed {

enum status_code : std::uint32_t {
  // Success
  success = 0,
  success_host_already_initialized = 0x00000001,
  success_different_runtime_properties = 0x00000002,

  // failure
  invalid_arg_failure = 0x80008081,
  core_host_lib_load_failure = 0x80008082,
  core_host_lib_missing_failure = 0x80008083,
  core_host_entry_point_failure = 0x80008084,
  core_host_cur_host_find_failure = 0x80008085,

  core_clr_resolve_failure = 0x80008087,
  core_clr_bind_failure = 0x80008088,
  core_clr_init_failure = 0x80008089,
  core_clr_exe_failure = 0x8000808a,
  resolver_init_failure = 0x8000808b,
  resolver_resolve_failure = 0x8000808c,
  lib_host_cur_exe_find_failure = 0x8000808d,
  lib_host_init_failure = 0x8000808e,

  lib_host_exec_mode_failure = 0x80008090,
  lib_host_sdk_find_failure = 0x80008091,
  lib_host_invalid_args = 0x80008092,
  invalid_config_file = 0x80008093,
  app_arg_not_runnable = 0x80008094,
  app_host_exe_not_bound_failure = 0x80008095,
  framework_missing_failure = 0x80008096,
  host_api_failed = 0x80008097,
  host_api_buffer_too_small = 0x80008098,
  lib_host_unknown_command = 0x80008099,
  lib_host_app_root_find_failure = 0x8000809a,
  sdk_resolver_resolve_failure = 0x8000809b,
  framework_compat_failure = 0x8000809c,
  framework_compat_retry = 0x8000809d,

  bundle_extraction_failure = 0x8000809f,
  bundle_extraction_io_error = 0x800080a0,
  lib_host_duplicate_property = 0x800080a1,
  host_api_unsupported_version = 0x800080a2,
  host_invalid_state = 0x800080a3,
  host_property_not_found = 0x800080a4,
  core_host_incompatible_config = 0x800080a5,
  host_api_unsupported_scenario = 0x800080a6,
  host_feature_disabled = 0x800080a7,
}; // enum status_code

struct core_clr_functions {
  hostfxr_set_error_writer_fn set_host_fxr_error_writer = nullptr;
  hostfxr_initialize_for_runtime_config_fn init_host_fxr_for_runtime_config = nullptr;
  hostfxr_get_runtime_delegate_fn get_runtime_delegate = nullptr;
  hostfxr_close_fn close_host_fxr = nullptr;
  load_assembly_and_get_function_pointer_fn get_managed_function_ptr = nullptr;
}; // struct core_clr_functions

static auto core_clr = core_clr_functions{};

message_callback_fn message_callback = nullptr;
message_level message_filter;
exception_callback_fn exception_callback = nullptr;

auto default_message_callback(std::string_view message, message_level level) -> void {
  auto level_str = std::string{""};

  switch (level) {
    case message_level::all:
    case message_level::info: {
      utility::logger<"scripting">::info("{}", message);
      break;
    }
    case message_level::warning: {
      utility::logger<"scripting">::warn("{}", message);
      break;
    }
    case message_level::error: {
      utility::logger<"scripting">::error("{}", message);
      break;
    }
  }
}

auto runtime::initialize(rumtime_config settings) -> runtime_status {
  if (!load_host_fxr()) {
    return runtime_status::dot_net_not_found;
  }

  _settings = std::move(settings);

  if (!_settings.message_callback) {
    _settings.message_callback = default_message_callback;
  }

  message_callback = _settings.message_callback;
  message_filter = _settings.message_filter;

  core_clr.set_host_fxr_error_writer([](const char_type* message) {
    auto message_str = string_helper::convert_wide_to_utf8(message);

    message_callback(message_str, message_level::error);
  });

  _managed_assembly_path = std::filesystem::path(_settings.backend_path) / "Sbx.Managed.dll";

  if (!std::filesystem::exists(_managed_assembly_path)) {
    message_callback(fmt::format("failed to find Sbx.Managed.dll in '{}'", _settings.backend_path), message_level::error);

    return runtime_status::managed_not_found;
  }

  if (!initialize_managed()) {
    return runtime_status::managed_init_error;
  }

  return runtime_status::success;
}

auto runtime::shutdown() -> void {
  core_clr.close_host_fxr(_host_fxr_context);
}

auto runtime::create_assembly_load_context(std::string_view name) -> assembly_load_context {
  auto context_name = string::create(name);

  auto context = assembly_load_context{};

  context._context_id = std::invoke(detail::backend.create_assembly_load_context, context_name);
  context._runtime = this;

  string::destroy(context_name);
  
  return context;
}

auto runtime::unload_assembly_load_context(assembly_load_context& load_context) -> void {
  std::invoke(detail::backend.unload_assembly_load_context, load_context._context_id);
  load_context._context_id = -1;
  load_context._loaded_assemblies.clear();
}

template<typename Function>
auto load_function_ptr(void* library_handle, const char* function_name) -> Function {
#if defined(SBX_WINDOWS)
  auto result = reinterpret_cast<Function>(GetProcAddress((HMODULE)library_handle, function_name));

  return result;
#else
  auto result = reinterpret_cast<Function>(dlsym(library_handle, function_name));

  return result;
#endif
}

auto get_host_fxr_path() -> std::filesystem::path {
#if defined(SBX_WINDOWS)
  std::filesystem::path base_path = "";
		

  TCHAR buffer[MAX_PATH];

  SHGetSpecialFolderPath(nullptr, buffer, CSIDL_PROGRAM_FILES, FALSE);

  base_path = buffer;
  base_path /= "dotnet/host/fxr/";

  auto search_paths = std::array{
    base_path
  };

#elif defined(SBX_UNIX)
  auto search_paths = std::array{
    std::filesystem::path("/usr/lib/dotnet/host/fxr/"),
    std::filesystem::path("/usr/share/dotnet/host/fxr/"),
  };
#endif

  for (const auto& path : search_paths) {
    if (!std::filesystem::exists(path)) {
      continue;
    }

    for (const auto& directory : std::filesystem::recursive_directory_iterator(path)) {
      if (!directory.is_directory()) {
        continue;
      }

      auto directory_path = directory.path().filename().string();

      if (!directory_path.starts_with(SBX_SCRIPTING_DOTNET_TARGET_VERSION_MAJOR_STR)) {
        continue;
      }

      return directory / std::filesystem::path(SBX_SCRIPTING_HOSTFXR_NAME );
    }
  }

  return "";
}

auto runtime::load_host_fxr() const -> bool {
  auto hostfxr_path = get_host_fxr_path();

  if (hostfxr_path.empty()) {
    return false;
  }

  // Load the CoreCLR library
  auto library_handle = static_cast<void*>(nullptr);

#ifdef SBX_WINDOWS
#ifdef SBX_SCRIPTING_WIDE_CHARS
  library_handle = LoadLibraryW(hostfxr_path.c_str());
#else
  library_handle = LoadLibraryA(hostfxr_path.string().c_str());
#endif
#else
  library_handle = dlopen(hostfxr_path.string().data(), RTLD_NOW | RTLD_GLOBAL);
#endif

  if (library_handle == nullptr) {
    return false;
  }

  // load core_clr functions
  core_clr.set_host_fxr_error_writer = load_function_ptr<hostfxr_set_error_writer_fn>(library_handle, "hostfxr_set_error_writer");
  core_clr.init_host_fxr_for_runtime_config = load_function_ptr<hostfxr_initialize_for_runtime_config_fn>(library_handle, "hostfxr_initialize_for_runtime_config");
  core_clr.get_runtime_delegate = load_function_ptr<hostfxr_get_runtime_delegate_fn>(library_handle, "hostfxr_get_runtime_delegate");
  core_clr.close_host_fxr = load_function_ptr<hostfxr_close_fn>(library_handle, "hostfxr_close");

  return true;
}

auto runtime::initialize_managed() -> bool {
  // Fetch load_assembly_and_get_function_pointer_fn from CoreCLR
  {
    auto runtime_config_path = std::filesystem::path(_settings.backend_path) / "Sbx.Managed.runtimeconfig.json";

    if (!std::filesystem::exists(runtime_config_path)) {
      message_callback("failed to find Sbx.Managed.runtimeconfig.json", message_level::error);

      return false;
    }

    core_clr.init_host_fxr_for_runtime_config(runtime_config_path.c_str(), nullptr, &_host_fxr_context);

    core_clr.get_runtime_delegate(_host_fxr_context, hdt_load_assembly_and_get_function_pointer, reinterpret_cast<void**>(&core_clr.get_managed_function_ptr));
  }

  using initialize_fn = void(*)(void(*)(string, message_level), void(*)(string));

  auto managed_entry_point = static_cast<initialize_fn>(nullptr);

  managed_entry_point = load_managed_function_ptr<initialize_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Host, Sbx.Managed"), SBX_SCRIPTING_STR("Initialize"));

  load_functions();

  managed_entry_point([](string message, message_level level) {
    if (message_filter & level) {
      auto message_str = std::string{message};

      message_callback(message_str, level);
    }
  },
  [](string message) {
    auto message_str = std::string{message};

    if (!exception_callback) {
      message_callback(message_str, message_level::error);
      return;
    }
    
    exception_callback(message_str);
  });

  exception_callback = _settings.exception_callback;

  return true;
}

auto runtime::load_functions() -> void {
  detail::backend.create_assembly_load_context = load_managed_function_ptr<detail::backend_functions::create_assembly_load_context_fn>(SBX_SCRIPTING_STR("Sbx.Managed.AssemblyLoader, Sbx.Managed"), SBX_SCRIPTING_STR("CreateAssemblyLoadContext"));
  detail::backend.unload_assembly_load_context = load_managed_function_ptr<detail::backend_functions::unload_assembly_load_context_fn>(SBX_SCRIPTING_STR("Sbx.Managed.AssemblyLoader, Sbx.Managed"), SBX_SCRIPTING_STR("UnloadAssemblyLoadContext"));
  detail::backend.load_assembly = load_managed_function_ptr<detail::backend_functions::load_assembly_fn>(SBX_SCRIPTING_STR("Sbx.Managed.AssemblyLoader, Sbx.Managed"), SBX_SCRIPTING_STR("LoadAssembly"));
  detail::backend.load_assembly_from_memory = load_managed_function_ptr<detail::backend_functions::load_assembly_from_memory_fn>(SBX_SCRIPTING_STR("Sbx.Managed.AssemblyLoader, Sbx.Managed"), SBX_SCRIPTING_STR("LoadAssemblyFromMemory"));
  detail::backend.unload_assembly_load_context = load_managed_function_ptr<detail::backend_functions::unload_assembly_load_context_fn>(SBX_SCRIPTING_STR("Sbx.Managed.AssemblyLoader, Sbx.Managed"), SBX_SCRIPTING_STR("UnloadAssemblyLoadContext"));
  detail::backend.get_last_load_status = load_managed_function_ptr<detail::backend_functions::get_last_load_status_fn>(SBX_SCRIPTING_STR("Sbx.Managed.AssemblyLoader, Sbx.Managed"), SBX_SCRIPTING_STR("GetLastLoadStatus"));
  detail::backend.get_assembly_name = load_managed_function_ptr<detail::backend_functions::get_assembly_name_fn>(SBX_SCRIPTING_STR("Sbx.Managed.AssemblyLoader, Sbx.Managed"), SBX_SCRIPTING_STR("GetAssemblyName"));

  detail::backend.get_assembly_types = load_managed_function_ptr<detail::backend_functions::get_assembly_types_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetAssemblyTypes"));
  detail::backend.get_type_id = load_managed_function_ptr<detail::backend_functions::get_type_id_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetTypeId"));
  detail::backend.get_full_type_name = load_managed_function_ptr<detail::backend_functions::get_full_type_name_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetFullTypeName"));
  detail::backend.get_assembly_qualified_name = load_managed_function_ptr<detail::backend_functions::get_assembly_qualified_name_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetAssemblyQualifiedName"));
  detail::backend.get_base_type = load_managed_function_ptr<detail::backend_functions::get_base_type_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetBaseType"));
  detail::backend.get_type_size = load_managed_function_ptr<detail::backend_functions::get_type_size_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetTypeSize"));
  detail::backend.is_type_subclass_of = load_managed_function_ptr<detail::backend_functions::is_type_subclass_of_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("IsTypeSubclassOf"));
  detail::backend.is_type_assignable_to = load_managed_function_ptr<detail::backend_functions::is_type_assignable_to_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("IsTypeAssignableTo"));
  detail::backend.is_type_assignable_from = load_managed_function_ptr<detail::backend_functions::is_type_assignable_from_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("IsTypeAssignableFrom"));
  detail::backend.is_type_sz_array = load_managed_function_ptr<detail::backend_functions::is_type_sz_array_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("IsTypeSzArray"));
  detail::backend.get_element_type = load_managed_function_ptr<detail::backend_functions::get_element_type_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetElementType"));
  detail::backend.get_type_methods = load_managed_function_ptr<detail::backend_functions::get_type_methods_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetTypeMethods"));
  detail::backend.get_type_fields = load_managed_function_ptr<detail::backend_functions::get_type_fields_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetTypeFields"));
  detail::backend.get_type_properties = load_managed_function_ptr<detail::backend_functions::get_type_properties_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetTypeProperties"));
  detail::backend.has_type_attribute = load_managed_function_ptr<detail::backend_functions::has_type_attribute_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("HasTypeAttribute"));
  detail::backend.get_type_attributes = load_managed_function_ptr<detail::backend_functions::get_type_attributes_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetTypeAttributes"));
  detail::backend.get_type_managed_type = load_managed_function_ptr<detail::backend_functions::get_type_managed_type_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetTypeManagedType"));
  detail::backend.invoke_static_method = load_managed_function_ptr<detail::backend_functions::invoke_static_method_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("InvokeStaticMethod"));
  detail::backend.invoke_static_method_return = load_managed_function_ptr<detail::backend_functions::invoke_static_method_return_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("InvokeStaticMethodRet"));

  detail::backend.get_method_info_name = load_managed_function_ptr<detail::backend_functions::get_method_info_name_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetMethodInfoName"));
  detail::backend.get_method_info_return_type = load_managed_function_ptr<detail::backend_functions::get_method_info_return_type_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetMethodInfoReturnType"));
  detail::backend.get_method_info_parameter_types = load_managed_function_ptr<detail::backend_functions::get_method_info_parameter_types_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetMethodInfoParameterTypes"));
  detail::backend.get_method_info_accessibility = load_managed_function_ptr<detail::backend_functions::get_method_info_accessibility_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetMethodInfoAccessibility"));
  detail::backend.get_method_info_attributes = load_managed_function_ptr<detail::backend_functions::get_method_info_attributes_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetMethodInfoAttributes"));

  detail::backend.get_field_info_name = load_managed_function_ptr<detail::backend_functions::get_field_info_name_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetFieldInfoName"));
  detail::backend.get_field_info_type = load_managed_function_ptr<detail::backend_functions::get_field_info_type_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetFieldInfoType"));
  detail::backend.get_field_info_accessibility = load_managed_function_ptr<detail::backend_functions::get_field_info_accessibility_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetFieldInfoAccessibility"));
  detail::backend.get_field_info_attributes = load_managed_function_ptr<detail::backend_functions::get_field_info_attributes_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetFieldInfoAttributes"));

  detail::backend.get_property_info_name = load_managed_function_ptr<detail::backend_functions::get_property_info_name_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetPropertyInfoName"));
  detail::backend.get_property_info_type = load_managed_function_ptr<detail::backend_functions::get_property_info_type_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetPropertyInfoType"));
  detail::backend.get_property_info_attributes = load_managed_function_ptr<detail::backend_functions::get_property_info_attributes_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetPropertyInfoAttributes"));

  detail::backend.get_attribute_field_value = load_managed_function_ptr<detail::backend_functions::get_attribute_field_value_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetAttributeFieldValue"));
  detail::backend.get_attribute_type = load_managed_function_ptr<detail::backend_functions::get_attribute_type_fn>(SBX_SCRIPTING_STR("Sbx.Managed.TypeInterface, Sbx.Managed"), SBX_SCRIPTING_STR("GetAttributeType"));

  detail::backend.set_internal_calls = load_managed_function_ptr<detail::backend_functions::set_internal_calls_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Interop.InternalCallsManager, Sbx.Managed"), SBX_SCRIPTING_STR("SetInternalCalls"));
  detail::backend.create_object = load_managed_function_ptr<detail::backend_functions::create_object_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("CreateObject"));
  detail::backend.invoke_method = load_managed_function_ptr<detail::backend_functions::invoke_method_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("InvokeMethod"));
  detail::backend.invoke_method_return = load_managed_function_ptr<detail::backend_functions::invoke_method_return_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("InvokeMethodRet"));
  detail::backend.set_field_value = load_managed_function_ptr<detail::backend_functions::set_field_value_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("SetFieldValue"));
  detail::backend.get_field_value = load_managed_function_ptr<detail::backend_functions::get_field_value_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("GetFieldValue"));
  detail::backend.set_property_value = load_managed_function_ptr<detail::backend_functions::set_property_value_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("SetPropertyValue"));
  detail::backend.get_property_value = load_managed_function_ptr<detail::backend_functions::get_property_value_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("GetPropertyValue"));
  detail::backend.destroy_object = load_managed_function_ptr<detail::backend_functions::destroy_object_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("DestroyObject"));
  detail::backend.get_object_type_id = load_managed_function_ptr<detail::backend_functions::get_object_type_id_fn>(SBX_SCRIPTING_STR("Sbx.Managed.Object, Sbx.Managed"), SBX_SCRIPTING_STR("GetObjectTypeId"));

  detail::backend.collect_garbage = load_managed_function_ptr<detail::backend_functions::collect_garbage_fn>(SBX_SCRIPTING_STR("Sbx.Managed.GarbageCollector, Sbx.Managed"), SBX_SCRIPTING_STR("CollectGarbage"));
  detail::backend.wait_for_pending_finalizers = load_managed_function_ptr<detail::backend_functions::wait_for_pending_finalizers_fn>(SBX_SCRIPTING_STR("Sbx.Managed.GarbageCollector, Sbx.Managed"), SBX_SCRIPTING_STR("WaitForPendingFinalizers"));
}


auto runtime::load_managed_function_ptr(const std::filesystem::path& assembly_path, const char_type* type_name, const char_type* method_name, const char_type* delegate_type) const -> void* {
  auto func_ptr = static_cast<void*>(nullptr);

  core_clr.get_managed_function_ptr(assembly_path.c_str(), type_name, method_name, delegate_type, nullptr, &func_ptr);
  
  return func_ptr;
}

} // namespace sbx::scripting::managed
