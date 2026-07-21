// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CORE_ENGINE_HPP_
#define LIBSBX_CORE_ENGINE_HPP_

#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/concepts.hpp>

#include <libsbx/units/units.hpp>

#include <libsbx/core/application.hpp>
#include <libsbx/core/module.hpp>

namespace sbx::core {

/**
 * @brief The engine core: timing state, cli args, application ownership and the static access surface. 
 * Driven by @ref basic_engine, which inherits from this class and provides the module composition.
 */
class engine : public utility::noncopyable {

public:

  [[nodiscard]] static auto delta_time() -> units::seconds;

  [[nodiscard]] static auto fixed_delta_time() -> units::seconds;

  [[nodiscard]] static auto time() -> units::seconds;

  [[nodiscard]] static auto args() -> const std::vector<std::string_view>&;

  static auto quit() -> void;

  /**
   * @brief Access to a module owned by the running engine.
   *
   * Valid from the moment the module is constructed until it is destroyed,
   * which means a module may access all modules listed before it already in
   * its constructor.
   *
   * @tparam Module The module type to access. Must be part of the running engine's module composition.
   *
   * @return A reference to the module instance.
   */
  template<module Module>
  [[nodiscard]] static auto get_module() -> Module&;

  /**
   * @brief Access to the application owned by the running engine.
   *
   * @tparam Application The application type to cast to. Must be the same as or derived from the base @ref core::application type.
   *
   * @return A reference to the application instance.
   */
  template<typename Application = core::application>
  requires (std::is_same_v<core::application, Application> || std::is_base_of_v<core::application, Application>)
  [[nodiscard]] static auto get_application() -> Application&;

protected:

  explicit engine(std::span<std::string_view> args);

  ~engine();

  static engine* _instance;

  units::seconds _delta_time{};
  units::seconds _time{};

  bool _is_running{};

  std::vector<std::string_view> _args{};

  std::unique_ptr<application> _application{};

}; // class engine

template<module... Modules>
using module_list = utility::type_list<Modules...>;

template<typename ModuleList>
requires (utility::is_type_list_v<ModuleList>)
class basic_engine;

/**
 * @brief The composed engine: an explicit, ordered list of modules driving
 * the @ref engine core.
 *
 * The list order is the construction order and the update order within each
 * stage; destruction runs in reverse. Every module's `dependencies` must
 * appear before the module itself.
 */
template<module... Modules>
requires (utility::are_unique_v<Modules...> && detail::dependencies_ordered_v<Modules...>)
class basic_engine<module_list<Modules...>> final : public engine {

public:

  explicit basic_engine(std::span<std::string_view> args);

  ~basic_engine() = default;

  template<typename Application, typename... Args>
  requires (std::is_base_of_v<core::application, Application> && std::is_constructible_v<Application, Args...>)
  auto run(Args&&... args) -> void;

private:

  template<stage Stage>
  auto _dispatch() -> void;

  auto _loop() -> void;

  module_storage<Modules...> _modules;

}; // class basic_engine

} // namespace sbx::core

#include <libsbx/core/engine.ipp>

#endif // LIBSBX_CORE_ENGINE_HPP_
