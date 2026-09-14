// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_manifest.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/hash.hpp>

#include <libsbx/core/engine.hpp>

namespace sbx::assets {

asset_manifest::~asset_manifest() {
  _save_manifest();
}

auto asset_manifest::import(const std::filesystem::path& path) -> math::uuid {
  ensure_loaded();

  const auto key = path.generic_string();

  if (const auto entry = _uuids.find(key); entry != _uuids.end()) {
    return entry->second;
  }

  const auto uuid = _read_or_create_meta(path);

  _uuids.emplace(key, uuid);
  _paths.emplace(uuid, path);

  auto& entry = _manifest[uuid];
  if (entry.path.empty()) {
    entry.path = path;
    _manifest_dirty = true;
  }

  return uuid;
}

auto asset_manifest::import_directory(const std::filesystem::path& root) -> void {
  ensure_loaded();

  if (!std::filesystem::exists(root)) {
    utility::logger<"assets">::warn("Asset root '{}' does not exist", root.generic_string());

    return;
  }

  for (const auto& entry : std::filesystem::recursive_directory_iterator{root}) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const auto& path = entry.path();

    auto extension = path.extension().string();

    std::ranges::transform(extension, extension.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".gltf" || extension == ".glb" || extension == ".material" || extension == ".hdr" || extension == ".particle_effect" || extension == ".animation_graph" || extension == ".shadergraph") {
      import(entry.path());
    }
  }
}

auto asset_manifest::path_of(const math::uuid& id) const -> std::filesystem::path {
  if (const auto entry = _paths.find(id); entry != _paths.end()) {
    return entry->second;
  }

  return {};
}

auto asset_manifest::ensure_loaded() -> void {
  if (_manifest_loaded) {
    return;
  }

  _manifest_loaded = true;

  _load_manifest();
}

auto asset_manifest::absolute(const std::filesystem::path& relative) -> std::filesystem::path {
  const auto& project = core::engine::project();

  return project.assets_directory() / relative;
}

// Inverse of absolute(): converts a resolved path back to one relative to assets_directory(),
// for storing assets-relative paths (e.g. in a .material file's texture slots).
auto asset_manifest::relative(const std::filesystem::path& absolute) -> std::filesystem::path {
  const auto& project = core::engine::project();

  return std::filesystem::relative(absolute, project.assets_directory());
}

auto asset_manifest::cooked_path(const math::uuid& id, std::string_view extension) const -> std::filesystem::path {
  const auto& project = core::engine::project();

  return project.library_directory() / fmt::format("{}{}", id.value(), extension);
}

auto asset_manifest::is_cooked_stale(const math::uuid& id, const std::filesystem::path& source, const std::filesystem::path& cooked, std::uint32_t cooker_version) -> bool {
  if (!std::filesystem::exists(cooked)) {
    return true;
  }

  const auto entry = _manifest.find(id);

  if (entry == _manifest.end() || entry->second.cooker_version != cooker_version) {
    return true;
  }

  auto error = std::error_code{};
  const auto mtime = std::filesystem::last_write_time(source, error);

  if (error) {
    return true;
  }

  const auto mtime_count = mtime.time_since_epoch().count();

  if (entry->second.source_mtime == mtime_count) {
    return false; // fast path: unchanged since last cook
  }

  // mtime moved — confirm with a content hash before recooking.
  if (entry->second.source_hash == utility::hash_file(source)) {
    entry->second.source_mtime = mtime_count; // touched, not changed
    _manifest_dirty = true;
    return false;
  }

  return true;
}

auto asset_manifest::record_cook(const math::uuid& id, std::uint32_t cooker_version, const std::filesystem::path& source) -> void {
  auto error = std::error_code{};
  const auto mtime = std::filesystem::last_write_time(source, error);
  const auto mtime_count = error ? std::int64_t{0} : mtime.time_since_epoch().count();
  const auto hash = utility::hash_file(source);

  auto& entry = _manifest[id];
  entry.cooker_version = cooker_version;
  entry.source_hash = hash;
  entry.source_mtime = mtime_count;
  _manifest_dirty = true;

  _save_manifest();
}

auto asset_manifest::_manifest_path() const -> std::filesystem::path {
  return core::engine::project().library_directory() / "manifest.yaml";
}

auto asset_manifest::_load_manifest() -> void {
  const auto path = _manifest_path();

  if (!std::filesystem::exists(path)) {
    return;
  }

  auto root = YAML::Node{};

  try {
    root = YAML::LoadFile(path.string());
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Could not read asset manifest '{}' ({})", path.generic_string(), exception.what());
    return;
  }

  const auto assets = root["assets"];

  if (!assets) {
    return;
  }

  for (const auto node : assets) {
    const auto uuid = node["uuid"].as<math::uuid>();

    auto entry = manifest_entry{};
    entry.path = node["path"].as<std::string>();
    entry.cooker_version = node["cooker_version"].as<std::uint32_t>();
    entry.source_hash = node["source_hash"].as<std::uint64_t>();
    entry.source_mtime = node["source_mtime"].as<std::int64_t>();

    _uuids.emplace(entry.path.generic_string(), uuid);
    _paths.emplace(uuid, entry.path);
    _manifest.emplace(uuid, std::move(entry));
  }

  utility::logger<"assets">::debug("Loaded asset manifest: {} entries", _manifest.size());
}

auto asset_manifest::_save_manifest() -> void {
  if (!_manifest_dirty) {
    return;
  }

  auto emitter = YAML::Emitter{};

  emitter << YAML::BeginMap;
  emitter << YAML::Key << "version" << YAML::Value << 1u;
  emitter << YAML::Key << "assets" << YAML::Value << YAML::BeginSeq;

  for (const auto& [uuid, entry] : _manifest) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "uuid" << YAML::Value << uuid.value();
    emitter << YAML::Key << "path" << YAML::Value << entry.path.generic_string();
    emitter << YAML::Key << "cooker_version" << YAML::Value << entry.cooker_version;
    emitter << YAML::Key << "source_hash" << YAML::Value << entry.source_hash;
    emitter << YAML::Key << "source_mtime" << YAML::Value << entry.source_mtime;
    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;
  emitter << YAML::EndMap;

  auto error = std::error_code{};
  std::filesystem::create_directories(_manifest_path().parent_path(), error);

  auto out = std::ofstream{_manifest_path()};
  out << emitter.c_str();

  _manifest_dirty = false;
}

auto asset_manifest::move(const std::filesystem::path& old_path, const std::filesystem::path& new_path, std::vector<std::pair<std::filesystem::path, std::filesystem::path>>& moved_assets) -> bool {
  ensure_loaded();

  auto ec = std::error_code{};
  std::filesystem::rename(old_path, new_path, ec);

  if (ec) {
    return false;
  }

  // Remaps one manifested file's index entry from `from` to `to` -- its .meta (identity) has
  // already physically moved to sit next to `to` by this point, either by the single-file
  // rename below or, for a directory, as part of the whole-subtree rename above.
  const auto remap_one = [this, &moved_assets](const std::filesystem::path& from, const std::filesystem::path& to) {
    const auto entry = _uuids.find(from.generic_string());

    if (entry == _uuids.end()) {
      return; // never imported -- nothing indexed to remap
    }

    const auto uuid = entry->second;
    _uuids.erase(entry);
    _uuids.emplace(to.generic_string(), uuid);
    _paths[uuid] = to;

    if (auto manifest_entry = _manifest.find(uuid); manifest_entry != _manifest.end()) {
      manifest_entry->second.path = to;
    }

    moved_assets.emplace_back(relative(from), relative(to));
    _manifest_dirty = true;
  };

  auto is_directory_ec = std::error_code{};
  const auto is_directory = std::filesystem::is_directory(new_path, is_directory_ec);

  if (!is_directory) {
    auto old_meta = old_path;
    old_meta += ".meta";

    if (std::filesystem::exists(old_meta)) {
      auto new_meta = new_path;
      new_meta += ".meta";

      auto meta_ec = std::error_code{};
      std::filesystem::rename(old_meta, new_meta, meta_ec);
    }

    remap_one(old_path, new_path);
  } else {
    // Every manifested file's index key starting with old_path/ moved along with the directory
    // rename above -- collect them first (mutating _uuids mid-iteration below would invalidate
    // this loop), then remap each in a second pass.
    const auto old_prefix = old_path.generic_string() + "/";

    auto affected = std::vector<std::pair<std::filesystem::path, std::filesystem::path>>{};

    for (const auto& [key, uuid] : _uuids) {
      if (key.starts_with(old_prefix)) {
        auto new_child = new_path / std::filesystem::path{key.substr(old_prefix.size())};
        affected.emplace_back(std::filesystem::path{key}, std::move(new_child));
      }
    }

    for (const auto& [from, to] : affected) {
      remap_one(from, to);
    }
  }

  _save_manifest();

  return true;
}

auto asset_manifest::remove(const std::filesystem::path& path) -> void {
  ensure_loaded();

  auto ec = std::error_code{};
  const auto is_directory = std::filesystem::is_directory(path, ec);

  if (is_directory) {
    const auto prefix = path.generic_string() + "/";

    for (const auto& [key, uuid] : _uuids) {
      if (key.starts_with(prefix)) {
        _paths.erase(uuid);
        _manifest.erase(uuid);
        _manifest_dirty = true;
      }
    }

    std::erase_if(_uuids, [&prefix](const auto& item) { return item.first.starts_with(prefix); });

    std::filesystem::remove_all(path, ec);
  } else {
    const auto entry = _uuids.find(path.generic_string());

    if (entry != _uuids.end()) {
      const auto uuid = entry->second;
      _uuids.erase(entry);
      _paths.erase(uuid);
      _manifest.erase(uuid);
      _manifest_dirty = true;
    }

    auto meta_path = path;
    meta_path += ".meta";
    std::filesystem::remove(meta_path, ec);

    std::filesystem::remove(path, ec);
  }

  _save_manifest();
}

auto asset_manifest::_read_or_create_meta(const std::filesystem::path& path) -> math::uuid {
  auto meta_path = path;
  meta_path += ".meta";

  if (std::filesystem::exists(meta_path)) {
    try {
      const auto node = YAML::LoadFile(meta_path.string());
      return node["uuid"].as<math::uuid>();
    } catch (const std::exception& exception) {
      utility::logger<"assets">::warn("Invalid meta '{}' ({}); regenerating", meta_path.generic_string(), exception.what());
    }
  }

  const auto uuid = math::uuid::create();

  auto node = YAML::Node{};
  node["uuid"] = uuid;

  auto out = std::ofstream{meta_path};
  out << node;

  utility::logger<"assets">::debug("Imported '{}' as {}", path.generic_string(), uuid);

  return uuid;
}

} // namespace sbx::assets
