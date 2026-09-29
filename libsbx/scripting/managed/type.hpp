// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCRIPTING_MANAGED_TYPE_HPP_
#define LIBSBX_SCRIPTING_MANAGED_TYPE_HPP_

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <libsbx/scripting/managed/core.hpp>
#include <libsbx/scripting/managed/string.hpp>
#include <libsbx/scripting/managed/object.hpp>
#include <libsbx/scripting/managed/fwd.hpp>

namespace sbx::scripting::managed {

namespace detail {

// Lets _method_handles (below) be looked up by std::string_view without constructing a temporary
// std::string on every lookup -- the whole point of caching a method handle is to keep the hot
// (cache-hit) path allocation-free.
struct transparent_string_hash {

  using is_transparent = void;

  auto operator()(std::string_view value) const noexcept -> std::size_t {
    return std::hash<std::string_view>{}(value);
  }

}; // struct transparent_string_hash

} // namespace detail

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

  auto get_full_name() const -> std::string;

  /** @brief Every enumerator's name and value, in declaration order -- empty if this isn't an enum type. */
  auto get_enum_entries() const -> std::vector<std::pair<std::string, std::int64_t>>;
  
  auto get_base_type() -> type&;

  auto get_type_id() const -> type_id;

  auto get_fields() -> std::vector<field_info>;

  auto is_subclass_of(type& other) -> bool;

  auto operator==(const type& other) const -> bool;

  operator bool() const;

  template<typename... Args>
  auto create_instance(Args&&... args) const -> object {
    constexpr auto parameter_count = sizeof...(args);

    auto result = object{};

    if constexpr (parameter_count > 0) {
      auto storage = std::tuple<std::decay_t<Args>...>{std::forward<Args>(args)...};

      auto parameter_values = std::array<const void*, parameter_count>{};
      auto parameter_types = std::array<managed_type, parameter_count>{};

      add_to_array(parameter_values, parameter_types, storage, std::make_index_sequence<parameter_count>{});

      result = _create_instance_internal(parameter_values.data(), parameter_types.data(), parameter_count);
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

  // object::invoke()'s method-handle cache -- see its doc comment. Keyed by method name only, not
  // full signature: fine for this engine's own dispatch surface (each dispatched name -- OnUpdate,
  // OnClick, DispatchCollisionEnter, ... -- is always called with one fixed signature), but a user
  // script overloading a method under the same name it also invokes via object::invoke() with
  // different signatures would collide on this cache. Not a concern for anything the engine itself
  // dispatches; flagged here in case a future caller needs the fuller (name, signature) key instead.
  mutable std::unordered_map<std::string, std::int32_t, detail::transparent_string_hash, std::equal_to<>> _method_handles{};

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
