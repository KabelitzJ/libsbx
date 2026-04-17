// SPDX-License-Identifier: MIT
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
  
private:

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