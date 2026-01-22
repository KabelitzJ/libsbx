// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCRIPTING_MANAGED_TYPE_HPP_
#define LIBSBX_SCRIPTING_MANAGED_TYPE_HPP_

#include <libsbx/scripting/managed/core.hpp>
#include <libsbx/scripting/managed/string.hpp>
#include <libsbx/scripting/managed/object.hpp>

namespace sbx::scripting::managed {

class type {

  friend class host_instance;
  friend class assembly;
  friend class assembly_load_context;
  friend class method_info;
  friend class field_info;
  friend class property_info;
  friend class attribute;
  friend class reflection_type;
  friend class object;

public:

  auto get_full_name() const -> string;
  
  auto get_base_type() -> type&;

  auto get_type_id() const -> type_id;

  auto operator==(const type& other) const -> bool;

  operator bool() const;

  template<typename... Args>
  auto create_instance(Args&&... args) const -> object {
    constexpr auto argument_count = sizeof...(args);

    auto result = object{};

    if constexpr (argument_count > 0) {
      const auto arguments = std::array<void*, argument_count>{};
      auto argument_types = std::array<managed_type, argument_count>{};

      add_to_array<Args...>(arguments, argument_types, std::forward<Args>(args)..., std::make_index_sequence<argument_count>{});

      result = _create_instance_internal(arguments.data(), argument_types.data(), argument_count);
    } else {
      result = _create_instance_internal(nullptr, nullptr, 0);
    }

    return result;
  }

private:

  auto _create_instance_internal(const void** parameters, const managed_type* parameter_types, std::size_t length) const -> object;

  type_id _id = -1;
  type* _base_type = nullptr;
  type* _element_type = nullptr;

}; // class type

class reflection_type {

public:

  operator type&() const;

public:

  type_id _id = -1;

}; // class reflection_type

static_assert(offsetof(reflection_type, _id) == 0);
static_assert(sizeof(reflection_type) == 4);

} // namespace sbx::scripting::managed  

#endif // LIBSBX_SCRIPTING_MANAGED_TYPE_HPP_