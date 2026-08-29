// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCRIPTING_MANAGED_ASSEMBLY_HPP_
#define LIBSBX_SCRIPTING_MANAGED_ASSEMBLY_HPP_

#include <vector>

#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/containers/stable_vector.hpp>

#include <libsbx/scripting/managed/fwd.hpp>
#include <libsbx/scripting/managed/core.hpp>
#include <libsbx/scripting/managed/platform.hpp>
#include <libsbx/scripting/managed/type.hpp>

namespace sbx::scripting::managed {

enum class assembly_load_status : std::uint8_t {
  success,
  file_not_found,
  file_load_failure,
  invalid_file_path,
  invalid_assembly,
  unknown_error
}; // enum class assembly_load_status

class assembly {

  friend class host_instance;
  friend class assembly_load_context;

public:

  auto get_assembly_id() const -> std::int32_t;

  auto get_load_status() const -> assembly_load_status;

  auto get_name() const -> std::string_view;

  auto add_internal_call(std::string_view class_name, std::string_view variable_name, void* function_pointer) -> void;

  auto upload_internal_calls() -> void;

  auto get_type(std::string_view class_name) const -> type&;

  auto get_types() const -> const std::vector<type*>&;

  /**
   * @brief Re-fetches this assembly's types from the backend and re-registers them with
   * detail::type_cache — without reloading the assembly itself (get_assembly_id() is unchanged).
   *
   * type_cache is a single process-wide cache (see its doc comment), so unloading any one
   * assembly_load_context wipes every assembly's cached types, including ones that were never
   * unloaded — leaving their get_types()/get_type() results stale (dangling type* / not-found).
   * Call this on every assembly that stays alive across such an unload (e.g. scripting_module's
   * long-lived Sbx.Core assembly, right after unloading the separate game-scripts context) to
   * repair it. A no-op-ish refresh otherwise: the underlying CLR Type objects are unchanged, so
   * this restores the exact same type ids/names, just re-cached.
   */
  auto reload_types() -> void;

private:

  auto _populate_types() -> void;

  runtime* _runtime = nullptr;
  std::int32_t _assembly_id = -1;
  assembly_load_status _load_status = assembly_load_status::unknown_error;
  std::string _name;

  std::vector<string_type> _internal_call_name_storage;
  
  std::vector<internal_call> _internal_calls;

  std::vector<type*> _types;

}; // class assembly

class assembly_load_context {

  friend class runtime;

public:

  auto load_assembly(std::string_view file_path) -> assembly&;

  auto load_assembly_from_memory(const std::byte* data, std::int64_t data_length) -> assembly&;

  auto get_or_load_assembly(std::string_view file_path) -> assembly&;

  auto get_loaded_assemblies() const -> const containers::stable_vector<assembly>&;

private:

  std::int32_t _context_id;
  containers::stable_vector<assembly> _loaded_assemblies;
  std::unordered_map<utility::hashed_string, std::uint32_t> _assembly_indices;

  runtime* _runtime = nullptr;

}; // class assembly_load_context

}; // namespace sbx::scripting::managed

#endif // LIBSBX_SCRIPTING_MANAGED_ASSEMBLY_HPP_