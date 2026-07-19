// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CORE_MODULE_HPP_
#define LIBSBX_CORE_MODULE_HPP_

#include <concepts>
#include <cstdint>
#include <functional>
#include <type_traits>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/type_list.hpp>

namespace sbx::core {

enum class stage : std::uint8_t {
  pre,
  update,
  post,
  fixed,
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

/**
 * @brief A module is any default-constructible class. Stage participation is
 * opt-in by defining any of the hooks:
 *
 * - `auto pre_update() -> void`
 * - `auto update() -> void`
 * - `auto post_update() -> void`
 * - `auto fixed_update() -> void`
 * - `auto render() -> void`
 *
 * Frame timing is available through `core::engine::delta_time()` and friends;
 * fixed update hooks must use `core::engine::fixed_delta_time()`.
 */
template<typename Type>
concept module = std::is_class_v<Type> && std::default_initializable<Type>;

template<typename Module>
concept has_pre_update = requires(Module& module) {
  { module.pre_update() } -> std::same_as<void>;
}; // concept has_pre_update

template<typename Module>
concept has_update = requires(Module& module) {
  { module.update() } -> std::same_as<void>;
}; // concept has_update

template<typename Module>
concept has_post_update = requires(Module& module) {
  { module.post_update() } -> std::same_as<void>;
}; // concept has_post_update

template<typename Module>
concept has_fixed_update = requires(Module& module) {
  { module.fixed_update() } -> std::same_as<void>;
}; // concept has_fixed_update

template<typename Module>
concept has_render = requires(Module& module) {
  { module.render() } -> std::same_as<void>;
}; // concept has_render

namespace detail {

/**
 * @brief Invokes the hook belonging to Stage if the module defines it.
 */
template<stage Stage, typename Module>
auto invoke_stage_hook(Module& module) -> void {
  if constexpr (Stage == stage::pre) {
    if constexpr (has_pre_update<Module>) {
      module.pre_update();
    }
  } else if constexpr (Stage == stage::update) {
    if constexpr (has_update<Module>) {
      module.update();
    }
  } else if constexpr (Stage == stage::post) {
    if constexpr (has_post_update<Module>) {
      module.post_update();
    }
  } else if constexpr (Stage == stage::fixed) {
    if constexpr (has_fixed_update<Module>) {
      module.fixed_update();
    }
  } else if constexpr (Stage == stage::render) {
    if constexpr (has_render<Module>) {
      module.render();
    }
  }
}

template<typename Module>
struct module_instance {
  inline static Module* pointer = nullptr;
}; // struct module_instance

template<typename Type, typename... Types>
inline constexpr auto contains_v = (std::is_same_v<Type, Types> || ...);

template<typename... Types>
struct are_unique : std::true_type { };

template<typename First, typename... Rest>
struct are_unique<First, Rest...> : std::bool_constant<!contains_v<First, Rest...> && are_unique<Rest...>::value> { };

template<typename... Types>
inline constexpr auto are_unique_v = are_unique<Types...>::value;

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
struct dependencies_in<dependency_list<Dependencies...>, Previous...> : std::bool_constant<(contains_v<Dependencies, Previous...> && ...)> { };

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
    module_instance<Module>::pointer = &instance;
  }

  ~module_slot() {
    module_instance<Module>::pointer = nullptr;
  }

  Module instance{};

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
    std::invoke(callable, slot.instance);
    rest.for_each(callable);
  }

  detail::module_slot<First> slot{};
  module_storage<Rest...> rest{};

}; // struct module_storage

} // namespace sbx::core

#endif // LIBSBX_CORE_MODULE_HPP_
