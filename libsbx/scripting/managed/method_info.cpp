// SPDX-License-Identifier: MIT
#include <libsbx/scripting/managed/method_info.hpp>

#include <libsbx/scripting/managed/detail/backend.hpp>
#include <libsbx/scripting/managed/detail/type_cache.hpp>

namespace sbx::scripting::managed {

auto method_info::get_name() const -> string {
  return std::invoke(detail::backend.get_method_info_name, _handle);
}

auto method_info::get_return_type() -> type& {
  if (!_return_type) {
    auto new_type = type{};

    std::invoke(detail::backend.get_method_info_return_type, _handle, &new_type._id);

    _return_type = detail::type_cache::get().cache_type(std::move(new_type));
  }

  return *_return_type;
}

auto method_info::get_parameter_types() -> const std::vector<type*>& {
  if (_parameter_types.empty()) {
    auto parameter_count = std::int32_t{};

    std::invoke(detail::backend.get_method_info_parameter_types, _handle, nullptr, &parameter_count);

    auto parameter_types = std::vector<type_id>{};
    parameter_types.resize(parameter_count);

    std::invoke(detail::backend.get_method_info_parameter_types, _handle, parameter_types.data(), &parameter_count);

    _parameter_types.resize(parameter_types.size());

    for (auto i = 0u; i < parameter_types.size(); ++i) {
      auto new_type = type{};
      new_type._id = parameter_types[i];
      _parameter_types[i] = detail::type_cache::get().cache_type(std::move(new_type));
    }
  }

  return _parameter_types;
}

auto method_info::get_accessibility() const -> type_accessibility {
  return std::invoke(detail::backend.get_method_info_accessibility, _handle);
}

auto method_info::get_attributes() const -> std::vector<attribute> {
  auto attribute_count = std::int32_t{};

  std::invoke(detail::backend.get_method_info_attributes, _handle, nullptr, &attribute_count);

  auto attribute_handles = std::vector<handle>{};
  attribute_handles.resize(attribute_count);

  std::invoke(detail::backend.get_method_info_attributes, _handle, attribute_handles.data(), &attribute_count);

  auto result = std::vector<attribute>{};
  result.resize(attribute_handles.size());

  for (auto i = 0u; i < attribute_handles.size(); ++i) {
    result[i]._handle = attribute_handles[i];
  }

  return result;
}


} // namespace sbx::scripting::managed
