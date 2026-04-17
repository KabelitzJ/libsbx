// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_VIEWPORT_REGISTRY_HPP_
#define LIBSBX_GRAPHICS_VIEWPORT_REGISTRY_HPP_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <libsbx/math/vector2.hpp>

#include <libsbx/signals/signal.hpp>

namespace sbx::graphics {

class viewport_registry {

public:

  using change_signal = signals::signal<const std::string&, const math::vector2u&>;

  auto declare(std::string name, const math::vector2u& initial_size) -> void {
    _sizes.insert_or_assign(name, initial_size);
    _dirty.insert(std::move(name));
  }

  auto resize(const std::string& name, const math::vector2u& size) -> bool {
    auto entry = _sizes.find(name);

    if (entry == _sizes.end()) {
      _sizes.emplace(name, size);
      _dirty.insert(name);
      _on_changed.emit(name, size);

      return true;
    }

    if (entry->second == size) {
      return false;
    }

    entry->second = size;
    _dirty.insert(name);
    _on_changed.emit(name, size);

    return true;
  }

  auto size(const std::string& name) const -> math::vector2u {
    if (auto entry = _sizes.find(name); entry != _sizes.end()) {
      return entry->second;
    }

    return math::vector2u{0u, 0u};
  }

  auto contains(const std::string& name) const -> bool {
    return _sizes.find(name) != _sizes.end();
  }

  auto take_dirty() -> std::vector<std::string> {
    auto result = std::vector<std::string>{_dirty.begin(), _dirty.end()};
    _dirty.clear();

    return result;
  }

  auto on_changed() -> change_signal& {
    return _on_changed;
  }

private:

  std::unordered_map<std::string, math::vector2u> _sizes;
  std::unordered_set<std::string> _dirty;
  change_signal _on_changed;

}; // class viewport_registry

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_VIEWPORT_REGISTRY_HPP_
