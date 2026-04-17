// SPDX-License-Identifier: MIT
#include <libsbx/scripting/managed/type.hpp>

#include <libsbx/scripting/managed/detail/backend.hpp>
#include <libsbx/scripting/managed/detail/type_cache.hpp>

namespace sbx::scripting::managed {

auto type::get_full_name() const -> string {
  return std::invoke(detail::backend.get_full_type_name, _id);
}

auto type::get_base_type() -> type& {
  if (!_base_type) {
    auto base_type = managed::type{};

    std::invoke(detail::backend.get_base_type, _id, &base_type._id);

    _base_type = detail::type_cache::get().cache_type(std::move(base_type));
  }

  return *_base_type;
}
  
auto type::get_type_id() const -> type_id {
  return _id;
}

auto type::operator==(const type& other) const -> bool {
  return _id == other._id;
}

type::operator bool() const {
  return _id != -1;
}

auto type::_create_instance_internal(const void** parameters, const managed_type* parameter_types, std::size_t length) const -> object {
  auto result = object{};

  result._handle = std::invoke(detail::backend.create_object, _id, false, parameters, parameter_types, static_cast<std::int32_t>(length));
  result._type = this;

  return result;
}

reflection_type::operator type&() const {
  static auto null_type = managed::type{};

  auto* result = detail::type_cache::get().get_type_by_id(_id);

  if (result == nullptr) {
    auto type = managed::type{};
    type._id = _id;
    result = detail::type_cache::get().cache_type(std::move(type));
  }

  return result != nullptr ? *result : null_type;
}

} // namespace sbx::scripting::managed