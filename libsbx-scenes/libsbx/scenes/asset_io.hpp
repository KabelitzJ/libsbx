// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_ASSET_IO_HPP_
#define LIBSBX_SCENES_ASSET_IO_HPP_

#include <functional>
#include <string>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/scenes/scene_asset_table.hpp>

namespace sbx::scenes {

class asset_io_registry {

public:

  template<std::invocable<scene_asset_table&, const utility::hashed_string&, const YAML::Node&> Load>
  auto register_loader(const std::string& category, Load&& load) -> void {
    _loaders[category] = std::forward<Load>(load);
  }

  auto has(const std::string& category) const -> bool {
    return _loaders.contains(category);
  }

  auto load(const std::string& category, scene_asset_table& assets, const utility::hashed_string& name, const YAML::Node& node) -> void {
    _loaders.at(category)(assets, name, node);
  }

private:

  std::unordered_map<std::string, std::function<void(scene_asset_table&, const utility::hashed_string&, const YAML::Node&)>> _loaders;

}; // class asset_io_registry

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_ASSET_IO_HPP_
