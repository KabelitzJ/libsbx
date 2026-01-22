// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CORE_MODULE_HPP_
#define LIBSBX_CORE_MODULE_HPP_

#include <type_traits>
#include <unordered_set>
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <memory>
#include <cinttypes>
#include <cmath>
#include <optional>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/type_id.hpp>

namespace sbx::core {

namespace detail {

struct core_type_id_scope { };

} // namespace detail

/**
 * @brief A scoped type ID generator for the libsbx-core scope.
 *
 * @tparam Type The type for which the ID is generated.
 */
template<typename Type>
using type_id = utility::scoped_type_id<detail::core_type_id_scope, Type>;

template<typename Derived, typename Base>
concept derived_from = std::is_base_of_v<Base, Derived>;

class module_manager {

  template<typename>
  friend class module;

  friend class engine;

public:

  module_manager() = delete;
  
  ~module_manager() = default;

private:

  enum class stage : std::uint8_t {
    pre,
    normal,
    post,
    pre_fixed,
    fixed,
    post_fixed,
    rendering
  }; // enum class stage

  template<typename... Types>
  struct dependencies {
    auto get() const noexcept -> std::unordered_set<std::uint32_t> {
      auto types = std::unordered_set<std::uint32_t>{};
      (types.insert(utility::type_id<Types>::value()), ...);
      return types;
    }
  }; // struct dependencies

  struct module_base {
    virtual ~module_base() = default;
    virtual auto update() -> void = 0;
  }; // struct module_base

  struct module_factory {
    module_manager::stage stage{};
    std::unordered_set<std::uint32_t> dependencies{};
    std::function<module_base*()> create{};
    std::function<void(module_base*)> destroy{};
  }; // module_factory

  static auto _factories() -> std::vector<std::optional<module_factory>>& {
    static auto instance = std::vector<std::optional<module_factory>>{};
    return instance;
  }

}; // class module_manager

template<typename Derived>
class module : public module_manager::module_base, public utility::noncopyable {

public:

  virtual ~module() {
    static_assert(!std::is_abstract_v<Derived>, "Class may not be abstract.");
    static_assert(std::is_base_of_v<module<Derived>, Derived>, "Class must inherit from module<Class>.");
  }

protected:

  using base_type = module_manager::module_base;

  template<typename... Dependencies>
  using dependencies = module_manager::dependencies<Dependencies...>;

  using stage = module_manager::stage;

  template<derived_from<base_type>... Dependencies>
  static auto register_module(stage stage, dependencies<Dependencies...>&& dependencies = {}) -> bool {
    const auto type = type_id<Derived>::value();

    auto& factories = module_manager::_factories();

    factories.resize(std::max(factories.size(), static_cast<std::size_t>(type + 1u)));

    factories[type] = module_manager::module_factory{
      .stage = stage,
      .dependencies = dependencies.get(),
      .create = [](){
        auto* instance = reinterpret_cast<Derived*>(std::malloc(sizeof(Derived)));

        if (!instance) {
          throw std::bad_alloc{};
        }

        std::construct_at(instance);

        return instance;
      },
      .destroy = [](module_base* instance){
        std::destroy_at(reinterpret_cast<Derived*>(instance));
        std::free(instance);
      }
    };

    return true;
  }

}; // class module

} // namespace sbx::core

#endif // LIBSBX_CORE_MODULE_HPP_
