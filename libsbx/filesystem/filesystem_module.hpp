// SPDX-License-Identifier: MIT
#ifndef LIBSBX_FILESYSTEM_FILESYSTEM_MODULE_HPP_
#define LIBSBX_FILESYSTEM_FILESYSTEM_MODULE_HPP_

#include <memory>
#include <string>
#include <vector>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/filesystem/alias.hpp>
#include <libsbx/filesystem/virtual_filesystem.hpp>
#include <libsbx/filesystem/filesystem_base.hpp>
#include <libsbx/filesystem/file_base.hpp>

namespace sbx::filesystem {

class filesystem_module : public core::module<filesystem_module> {
  inline static const auto is_registered = register_module(stage::pre);

public:

  filesystem_module();

  ~filesystem_module() override;

  auto update() -> void override;

  template<typename Type, typename... Args>
  requires (std::is_base_of_v<filesystem_base, Type>)
  auto create_filesystem(const alias& alias, Args&&... args) -> std::shared_ptr<Type> {
    return _filesystem->create_filesystem<Type>(alias, std::forward<Args>(args)...);
  }

  template<typename Type, typename... Args>
  auto create_filesystem(std::string&& alias, Args&&... args) -> std::shared_ptr<Type> {
    return _filesystem->create_filesystem<Type>(std::move(alias), std::forward<Args>(args)...);
  }

  auto open_file(const std::string& path, file_base::mode mode) -> file_ptr {
    return _filesystem->open_file(path, mode);
  }

  auto exists(const std::string& path) const -> bool {
    return _filesystem->exists(path);
  }

  auto all_files() const -> std::vector<std::string> {
    return _filesystem->all_files();
  }

  auto unregister_alias(const alias& alias) -> void {
    _filesystem->unregister_alias(alias);
  }

  auto is_alias_registered(const alias& alias) const -> bool {
    return _filesystem->is_alias_registered(alias);
  }

private:

  std::unique_ptr<virtual_filesystem> _filesystem;

}; // class filesystem_module

} // namespace sbx::filesystem

#endif // LIBSBX_FILESYSTEM_FILESYSTEM_MODULE_HPP_
