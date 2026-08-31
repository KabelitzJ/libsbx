// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CORE_MODULE_HPP_
#define LIBSBX_CORE_MODULE_HPP_

#include <concepts>
#include <cstdint>
#include <functional>
#include <meta>
#include <type_traits>

#include <libsbx/reflection/annotations.hpp>
#include <libsbx/reflection/enum.hpp>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/type_list.hpp>
#include <libsbx/utility/concepts.hpp>

#include <libsbx/memory/aligned_storage.hpp>

namespace sbx::core {

enum class [[=reflection::named]] stage : std::uint8_t {
  pre_update,
  update,
  post_update,
  fixed_update,
  late_update,
  render
}; // enum class stage

/**
 * @brief Declares the modules a module must be constructed after.
 *
 * Usage: `using dependencies = core::dependency_list<platform::platform_module>;`
 */
template<typename... Types>
struct dependency_list {
  using type = dependency_list;
  inline static constexpr auto size = sizeof...(Types);
}; // struct dependency_list

template<typename Type>
concept module = std::is_class_v<Type> && std::default_initializable<Type>;

namespace detail {

template<typename Module>
consteval auto find_hook(std::string_view name) -> std::meta::info {
  for (auto member : std::meta::members_of(^^Module, std::meta::access_context::current())) {
    auto matches = std::meta::is_function(member) 
      && std::meta::has_identifier(member) 
      && (std::meta::identifier_of(member) == name) 
      && (std::meta::return_type_of(member) == ^^void) 
      && std::meta::parameters_of(member).empty();

    if (matches) {
      return member;
    }
  }

  return std::meta::info{};
}

template<stage Stage, typename Module>
auto invoke_stage_hook(Module& module) -> void {
  constexpr auto member = find_hook<Module>(reflection::to_string(Stage));

  if constexpr (member != std::meta::info{}) {
    module.[:member:]();
  }
}

template<typename Module>
struct module_instance {
  inline static Module* pointer = nullptr;
}; // struct module_instance



template<typename Module>
struct module_dependencies {
  using type = dependency_list<>;
}; // struct module_dependencies

template<typename Module>
requires requires { typename Module::dependencies; }
struct module_dependencies<Module> {
  using type = typename Module::dependencies;
}; // struct module_dependencies

template<typename Module>
using module_dependencies_t = typename module_dependencies<Module>::type;

template<typename List, typename... Previous>
struct dependencies_in;

template<typename... Dependencies, typename... Previous>
struct dependencies_in<dependency_list<Dependencies...>, Previous...> : std::bool_constant<(utility::contains_v<Dependencies, Previous...> && ...)> { };

/**
 * @brief Checks that every module's dependencies appear before it in the list.
 */
template<typename Checked, typename... Rest>
struct dependencies_ordered;

template<typename... Checked>
struct dependencies_ordered<utility::type_list<Checked...>> : std::true_type { };

template<typename... Checked, typename First, typename... Rest>
struct dependencies_ordered<utility::type_list<Checked...>, First, Rest...>
: std::conditional_t<
    dependencies_in<module_dependencies_t<First>, Checked...>::value,
    dependencies_ordered<utility::type_list<Checked..., First>, Rest...>,
    std::false_type
  > { };

template<typename... Modules>
inline constexpr auto dependencies_ordered_v = dependencies_ordered<utility::type_list<>, Modules...>::value;

/**
 * @brief Owns a module and publishes its location for @ref engine::get_module.
 *
 * The pointer is published after the module's constructor completes, so
 * modules constructed later may already access it during their construction.
 */
template<module Module>
struct module_slot {

  module_slot() {
    std::construct_at(get());

    module_instance<Module>::pointer = get();
  }

  ~module_slot() noexcept {
    std::destroy_at(get());

    module_instance<Module>::pointer = nullptr;
  }

  auto get() noexcept -> Module* {
    return std::launder(reinterpret_cast<Module*>(&storage));
  }

  memory::storage_for_t<Module> storage{};

}; // struct module_slot

} // namespace detail

/**
 * @brief Stores modules with guaranteed in-order construction and reverse-order
 * destruction.
 */
template<module... Modules>
struct module_storage {

  template<typename Callable>
  auto for_each([[maybe_unused]] Callable&& callable) -> void { }

}; // struct module_storage

template<module First, module... Rest>
struct module_storage<First, Rest...> {

  template<typename Callable>
  auto for_each(Callable&& callable) -> void {
    std::invoke(callable, *slot.get());
    rest.for_each(callable);
  }

  detail::module_slot<First> slot{};
  module_storage<Rest...> rest{};

}; // struct module_storage

} // namespace sbx::core

#endif // LIBSBX_CORE_MODULE_HPP_
