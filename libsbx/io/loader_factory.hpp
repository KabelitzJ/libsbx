// SPDX-License-Identifier: MIT
#ifndef LIBSBX_IO_LOADER_FACTORY_HPP_
#define LIBSBX_IO_LOADER_FACTORY_HPP_

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

namespace sbx::io {

template<typename Type, typename Result>
class loader_factory {

  using load_function_type = std::function<Result(const std::filesystem::path&)>;
  using unload_function_type = std::function<void(Result&)>;

  struct function_handle_type {
    load_function_type load_function;
    unload_function_type unload_function;
  }; // struct function_handle_type

  using loader_container_type = std::unordered_map<std::string, function_handle_type>;

public:

  template<typename Derived>
  class loader {

  public:

    using result_type = Result;

  protected:

    template<typename... Extensions>
    static auto register_extensions(Extensions&&... extensions) -> bool {
      ((loader_factory<Type, Result>::_loaders()[extensions] = function_handle_type{
        .load_function = &Derived::load,
        .unload_function = &Derived::unload_wrapper
      }), ...);

      return true;
    }

    static auto unload_wrapper(Result& result) -> void {
      Derived::unload(result);
    }

    static auto unload(Result& result) -> void {
      static_cast<void>(result);
    }

  }; // struct loader

protected:

  static auto _loaders() -> loader_container_type& {
    static auto loaders = loader_container_type{};
    return loaders;
  }

}; // class loader_factory

} // namespace sbx::io

#endif // LIBSBX_IO_LOADER_FACTORY_HPP_
