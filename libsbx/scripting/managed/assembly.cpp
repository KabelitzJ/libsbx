// SPDX-License-Identifier: MIT
#include <libsbx/scripting/managed/assembly.hpp>

#include <format>

#include <libsbx/scripting/managed/detail/backend.hpp>
#include <libsbx/scripting/managed/detail/type_cache.hpp>

namespace sbx::scripting::managed {

auto assembly::get_assembly_id() const -> std::int32_t { 
  return _assembly_id; 
}

auto assembly::get_load_status() const -> assembly_load_status { 
  return _load_status; 
}

auto assembly::get_name() const -> std::string_view { 
  return _name; 
}

auto assembly::add_internal_call(std::string_view class_name, std::string_view variable_name, void* function_pointer) -> void {
  auto assembly_qualified_name = std::string{class_name};
  assembly_qualified_name += "+";
  assembly_qualified_name += variable_name;
  assembly_qualified_name += ", ";
  assembly_qualified_name += _name;

  const auto& name = _internal_call_name_storage.emplace_back(string_helper::convert_utf8_to_wide(assembly_qualified_name));

  auto call = internal_call{};
  call.name = name.c_str();
  call.native_function_pointer = function_pointer;

  _internal_calls.emplace_back(call);
}

auto assembly::upload_internal_calls() -> void {
  std::invoke(detail::backend.set_internal_calls, _internal_calls.data(), static_cast<std::int32_t>(_internal_calls.size()));
}

auto assembly::get_type(std::string_view class_name) const -> type& {
  static auto null_type = type{};

  auto* type = detail::type_cache::get().get_type_by_name(class_name);
  
  return type != nullptr ? *type : null_type;
}

auto assembly::get_types() const -> const std::vector<type*>& {
  return _types;
}

auto assembly_load_context::load_assembly(std::string_view file_path) -> assembly& {
  auto filepath = string::create(file_path);

  auto [idx, result] = _loaded_assemblies.emplace_back();

  _assembly_indices.emplace(utility::hashed_string{file_path.data(), file_path.size()}, idx);

  result._runtime = _runtime;
  result._assembly_id = std::invoke(detail::backend.load_assembly, _context_id, filepath);
  result._load_status = std::invoke(detail::backend.get_last_load_status);

  if (result._load_status == assembly_load_status::success) {
    auto assembly_name = std::invoke(detail::backend.get_assembly_name, result._assembly_id);

    result._name = assembly_name;

    string::destroy(assembly_name);

    auto type_count = std::int32_t{0};

    std::invoke(detail::backend.get_assembly_types, result._assembly_id, nullptr, &type_count);

    auto type_ids = std::vector<type_id>{};
    type_ids.resize(static_cast<std::size_t>(type_count));

    std::invoke(detail::backend.get_assembly_types, result._assembly_id, type_ids.data(), &type_count);

    for (auto type_id : type_ids) {
      auto new_type = type{};
      new_type._id = type_id;

      result._types.push_back(detail::type_cache::get().cache_type(std::move(new_type)));
    }
  }

  string::destroy(filepath);

  return result;
}

auto assembly_load_context::load_assembly_from_memory(const std::byte* data, std::int64_t data_length) -> assembly& {
  auto [idx, result] = _loaded_assemblies.emplace_back();
  
  result._runtime = _runtime;
  result._assembly_id = std::invoke(detail::backend.load_assembly_from_memory, _context_id, data, data_length);
  result._load_status = std::invoke(detail::backend.get_last_load_status);

  if (result._load_status == assembly_load_status::success) {
    auto assembly_name = std::invoke(detail::backend.get_assembly_name, result._assembly_id);

    result._name = assembly_name;

    string::destroy(assembly_name);

    auto type_count = std::int32_t{0};

    std::invoke(detail::backend.get_assembly_types, result._assembly_id, nullptr, &type_count);

    auto type_ids = std::vector<type_id>{};
    type_ids.resize(static_cast<std::size_t>(type_count));

    std::invoke(detail::backend.get_assembly_types, result._assembly_id, type_ids.data(), &type_count);

    for (auto type_id : type_ids) {
      auto new_type = type{};
      new_type._id = type_id;

      result._types.push_back(detail::type_cache::get().cache_type(std::move(new_type)));
    }
  }

  return result;
}

auto assembly_load_context::get_or_load_assembly(std::string_view file_path) -> assembly& {
  if (auto entry = _assembly_indices.find(utility::hashed_string{file_path.data(), file_path.size()}); entry != _assembly_indices.end()) {
    return _loaded_assemblies[entry->second];
  }

  return load_assembly(file_path);
}

auto assembly_load_context::get_loaded_assemblies() const -> const containers::stable_vector<assembly>& { 
  return _loaded_assemblies; 
}

}; // namespace sbx::scripting::managed