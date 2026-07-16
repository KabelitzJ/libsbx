// SPDX-License-Identifier: MIT
#include <libsbx/assets/serializer_registry.hpp>

#include <algorithm>
#include <cctype>

#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/logger.hpp>

namespace sbx::assets {

auto serializer_registry::install_registered() -> void {
  utility::logger<"assets">::info("installing {} importers", _pending_serializers().size());

  for (const auto& pending : _pending_serializers()) {
    auto instance = std::invoke(pending.factory);

    for (const auto& extension : pending.extensions) {
      _register_serializer(extension, instance);
    }
  }
}

auto serializer_registry::unregister(std::string_view extension) -> bool {
  return _by_extension.erase(_normalize(extension)) > 0u;
}

auto serializer_registry::_matched_extension(const std::filesystem::path& source) const -> const std::vector<std::shared_ptr<serializer_base>>* {
  auto filename = source.filename().string();

  std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char character) -> unsigned char {
    return static_cast<unsigned char>(std::tolower(character));
  });

  auto dot = filename.find('.');

  while (dot != std::string::npos) {
    const auto suffix = std::string_view{filename.data() + dot, filename.size() - dot};

    if (const auto entry = _by_extension.find(std::string{suffix}); entry != _by_extension.end()) {
      return &entry->second;
    }

    dot = filename.find('.', dot + 1u);
  }

  return nullptr;
}

auto serializer_registry::find_all_for(const std::filesystem::path& source) const -> std::span<const std::shared_ptr<serializer_base>> {
  if (const auto* serializers = _matched_extension(source); serializers) {
    return std::span<const std::shared_ptr<serializer_base>>{*serializers};
  }

  return {};
}

auto serializer_registry::find_for(const std::filesystem::path& source) const -> std::shared_ptr<serializer_base> {
  const auto serializers = find_all_for(source);

  if (serializers.empty()) {
    return nullptr;
  }

  return serializers.front();
}

auto serializer_registry::contains(std::string_view extension) const -> bool {
  return _by_extension.contains(_normalize(extension));
}

auto serializer_registry::clear() -> void {
  _by_extension.clear();
}

auto serializer_registry::_normalize(std::string_view extension) -> std::string {
  auto result = std::string{extension};

  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) -> unsigned char {
    return static_cast<unsigned char>(std::tolower(character));
  });

  return result;
}

auto serializer_registry::_register_serializer(std::string_view extension, std::shared_ptr<serializer_base> instance) -> void {
  if (extension.empty() || extension.front() != '.') {
    throw utility::runtime_error{"Serializer extension '{}' must start with a dot", extension};
  }

  if (!instance) {
    throw utility::runtime_error{"Refusing to register null importer for '{}'", extension};
  }

  _by_extension[_normalize(extension)].push_back(std::move(instance));
}

} // namespace sbx::assets
