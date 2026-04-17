// SPDX-License-Identifier: MIT
#include <libsbx/scripting/managed/field_info.hpp>

#include <libsbx/scripting/managed/detail/backend.hpp>
#include <libsbx/scripting/managed/detail/type_cache.hpp>

namespace sbx::scripting::managed {

auto field_info::get_name() const -> string {
  return std::invoke(detail::backend.get_field_info_name, _handle);
}

auto field_info::get_type() -> type& {
  if (!_type) {
    auto new_type = type{};

    std::invoke(detail::backend.get_field_info_type, _handle, &new_type._id);

    _type = detail::type_cache::get().cache_type(std::move(new_type));
  }

  return *_type;
}

auto field_info::get_accessibility() const -> type_accessibility {
  return std::invoke(detail::backend.get_field_info_accessibility, _handle);
}

auto field_info::get_attributes() const -> std::vector<attribute> {
  auto attribute_count = std::int32_t{};

  std::invoke(detail::backend.get_field_info_attributes, _handle, nullptr, &attribute_count);

  auto attribute_handles = std::vector<handle>{};
  attribute_handles.resize(static_cast<std::size_t>(attribute_count));

  std::invoke(detail::backend.get_field_info_attributes, _handle, attribute_handles.data(), &attribute_count);

  auto result = std::vector<attribute>{};
  result.resize(attribute_handles.size());
  
  for (auto i = 0u; i < attribute_handles.size(); ++i) {
    result[i]._handle = attribute_handles[i];
  }

  return result;
}

} // sbx