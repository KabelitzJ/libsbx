// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_DETAIL_BACKEND_HPP_
#define LIBSBX_SCRIPTING_DETAIL_BACKEND_HPP_

#include <libsbx/scripting/managed/core.hpp>
#include <libsbx/scripting/managed/string.hpp>
#include <libsbx/scripting/managed/garbage_collection.hpp>
#include <libsbx/scripting/managed/assembly.hpp>

namespace sbx::scripting::managed::detail {

struct backend_functions {

  using set_internal_calls_fn = void(*)(void*, std::int32_t);
	using create_assembly_load_context_fn = std::int32_t(*)(string);
	using unload_assembly_load_context_fn = void(*)(std::int32_t);
	using load_assembly_fn = std::int32_t(*)(std::int32_t, string);
	using load_assembly_from_memory_fn = std::int32_t(*)(std::int32_t, const std::byte*, std::int64_t);
	using get_last_load_status_fn = assembly_load_status(*)();
	using get_assembly_name_fn = string(*)(std::int32_t);

  using get_assembly_types_fn = void(*)(std::int32_t, type_id*, std::int32_t*);
	using get_type_id_fn = void(*)(string, type_id*);
	using get_full_type_name_fn = string(*)(type_id);
	using get_assembly_qualified_name_fn = string(*)(type_id);
	using get_base_type_fn = void(*)(type_id, type_id*);
	using get_type_size_fn = std::int32_t(*)(type_id);
	using is_type_subclass_of_fn = bool32(*)(type_id, type_id);
	using is_type_assignable_to_fn = bool32(*)(type_id, type_id);
	using is_type_assignable_from_fn = bool32(*)(type_id, type_id);
	using is_type_sz_array_fn = bool32(*)(type_id);
	using get_element_type_fn = void(*)(type_id, type_id*);
	using get_type_methods_fn = void(*)(type_id, handle*, std::int32_t*);
	using get_type_fields_fn = void(*)(type_id, handle*, std::int32_t*);
	using get_type_properties_fn = void(*)(type_id, handle*, std::int32_t*);
	using has_type_attribute_fn = bool32(*)(type_id, type_id);
	using get_type_attributes_fn = void(*)(handle, type_id*, std::int32_t*);
	using get_type_managed_type_fn = managed_type(*)(type_id);

  using get_method_info_name_fn = string(*)(handle);
	using get_method_info_return_type_fn = void(*)(handle, type_id*);
	using get_method_info_parameter_types_fn = void(*)(handle, type_id*, std::int32_t*);
	using get_method_info_accessibility_fn = type_accessibility(*)(handle);
	using get_method_info_attributes_fn = void(*)(handle, type_id*, std::int32_t*);

	using get_field_info_name_fn = string(*)(handle);
	using get_field_info_type_fn = void(*)(handle, type_id*);
	using get_field_info_accessibility_fn = type_accessibility(*)(handle);
	using get_field_info_attributes_fn = void(*)(handle, type_id*, std::int32_t*);

	using get_property_info_name_fn = string(*)(handle);
	using get_property_info_type_fn = void(*)(handle, type_id*);
	using get_property_info_attributes_fn = void(*)(handle, type_id*, std::int32_t*);

	using get_attribute_field_value_fn = void(*)(handle, string, void*);
	using get_attribute_type_fn = void(*)(handle, type_id*);

	using create_object_fn = void*(*)(type_id, bool32, const void**, const managed_type*, std::int32_t);
	using invoke_method_fn = void(*)(void*, string, const void**, const managed_type*, std::int32_t);
	using invoke_method_return_fn = void(*)(void*, string, const void**, const managed_type*, std::int32_t, void*);
	using invoke_static_method_fn = void(*)(type_id, string, const void**, const managed_type*, std::int32_t);
	using invoke_static_method_return_fn = void(*)(type_id, string, const void**, const managed_type*, std::int32_t, void*);
	using set_field_value_fn = void(*)(void*, string, const void*);
	using get_field_value_fn = void(*)(void*, string, void*);
	using set_property_value_fn = void(*)(void*, string, const void*);
	using get_property_value_fn = void(*)(void*, string, void*);
	using destroy_object_fn = void(*)(void*);
	using get_object_type_id_fn = void(*)(void*, std::int32_t*);

	using collect_garbage_fn = void(*)(std::int32_t, garbage_collection::mode, bool32, bool32);
	using wait_for_pending_finalizers_fn = void(*)();

  set_internal_calls_fn set_internal_calls{nullptr};
  load_assembly_fn load_assembly{nullptr};
  load_assembly_from_memory_fn load_assembly_from_memory{nullptr};
  unload_assembly_load_context_fn unload_assembly_load_context{nullptr};
  get_last_load_status_fn get_last_load_status{nullptr};
  get_assembly_name_fn get_assembly_name{nullptr};

  get_assembly_types_fn get_assembly_types{nullptr};
  get_type_id_fn get_type_id{nullptr};
  get_full_type_name_fn get_full_type_name{nullptr};
  get_assembly_qualified_name_fn get_assembly_qualified_name{nullptr};
  get_base_type_fn get_base_type{nullptr};
  get_type_size_fn get_type_size{nullptr};
  is_type_subclass_of_fn is_type_subclass_of{nullptr};
  is_type_assignable_to_fn is_type_assignable_to{nullptr};
  is_type_assignable_from_fn is_type_assignable_from{nullptr};
  is_type_sz_array_fn is_type_sz_array{nullptr};
  get_element_type_fn get_element_type{nullptr};
  get_type_methods_fn get_type_methods{nullptr};
  get_type_fields_fn get_type_fields{nullptr};
  get_type_properties_fn get_type_properties{nullptr};
  has_type_attribute_fn has_type_attribute{nullptr};
  get_type_attributes_fn get_type_attributes{nullptr};
  get_type_managed_type_fn get_type_managed_type{nullptr};

  get_method_info_name_fn get_method_info_name{nullptr};
  get_method_info_return_type_fn get_method_info_return_type{nullptr};
  get_method_info_parameter_types_fn get_method_info_parameter_types{nullptr};
  get_method_info_accessibility_fn get_method_info_accessibility{nullptr};
  get_method_info_attributes_fn get_method_info_attributes{nullptr};

  get_field_info_name_fn get_field_info_name{nullptr};
  get_field_info_type_fn get_field_info_type{nullptr};
  get_field_info_accessibility_fn get_field_info_accessibility{nullptr};
  get_field_info_attributes_fn get_field_info_attributes{nullptr};

  get_property_info_name_fn get_property_info_name{nullptr};
  get_property_info_type_fn get_property_info_type{nullptr};
  get_property_info_attributes_fn get_property_info_attributes{nullptr};

  get_attribute_field_value_fn get_attribute_field_value{nullptr};
  get_attribute_type_fn get_attribute_type{nullptr};

  create_object_fn create_object{nullptr};
  create_assembly_load_context_fn create_assembly_load_context{nullptr};
  invoke_method_fn invoke_method{nullptr};
  invoke_method_return_fn invoke_method_return{nullptr};
  invoke_static_method_fn invoke_static_method{nullptr};
  invoke_static_method_return_fn invoke_static_method_return{nullptr};
  set_field_value_fn set_field_value{nullptr};
  get_field_value_fn get_field_value{nullptr};
  set_property_value_fn set_property_value{nullptr};
  get_property_value_fn get_property_value{nullptr};
  destroy_object_fn destroy_object{nullptr};
  get_object_type_id_fn get_object_type_id{nullptr};

  collect_garbage_fn collect_garbage = nullptr;
  wait_for_pending_finalizers_fn wait_for_pending_finalizers = nullptr;

}; // struct backend_functions

inline auto backend = backend_functions{};
  
} // namespace sbx::scripting::managed::detail

#endif // LIBSBX_SCRIPTING_DETAIL_BACKEND_HPP_