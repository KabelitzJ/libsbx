// SPDX-License-Identifier: MIT
#include <libsbx/assets/importer_registry.hpp>

#include <algorithm>
#include <cctype>

#include <libsbx/utility/exception.hpp>

namespace sbx::assets {

auto importer_registry::register_for(std::string_view extension, std::shared_ptr<importer> instance) -> void {
  if (extension.empty() || extension.front() != '.') {
    throw utility::runtime_error{"Importer extension '{}' must start with a dot", extension};
  }

  if (!instance) {
    throw utility::runtime_error{"Refusing to register null importer for '{}'", extension};
  }

  _by_extension[_normalize(extension)] = std::move(instance);
}

auto importer_registry::register_for(std::initializer_list<std::string_view> extensions, std::shared_ptr<importer> instance) -> void {
  for (const auto& extension : extensions) {
    register_for(extension, instance);
  }
}

auto importer_registry::unregister(std::string_view extension) -> bool {
  return _by_extension.erase(_normalize(extension)) > 0u;
}

auto importer_registry::find_for(const std::filesystem::path& source) const -> std::shared_ptr<importer> {
  auto filename = source.filename().string();

  std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char character) -> unsigned char {
    return static_cast<unsigned char>(std::tolower(character));
  });

  auto dot = filename.find('.');

  while (dot != std::string::npos) {
    const auto suffix = std::string_view{filename.data() + dot, filename.size() - dot};

    if (const auto entry = _by_extension.find(std::string{suffix}); entry != _by_extension.end()) {
      return entry->second;
    }

    dot = filename.find('.', dot + 1u);
  }

  return nullptr;
}

auto importer_registry::find(std::string_view extension) const -> std::shared_ptr<importer> {
  if (const auto entry = _by_extension.find(_normalize(extension)); entry != _by_extension.end()) {
    return entry->second;
  }

  return nullptr;
}

auto importer_registry::contains(std::string_view extension) const -> bool {
  return _by_extension.contains(_normalize(extension));
}

auto importer_registry::clear() -> void {
  _by_extension.clear();
}

auto importer_registry::_normalize(std::string_view extension) -> std::string {
  auto result = std::string{extension};

  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) -> unsigned char {
    return static_cast<unsigned char>(std::tolower(character));
  });

  return result;
}

} // namespace sbx::assets
