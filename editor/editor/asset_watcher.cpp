// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/asset_watcher.hpp>

#include <system_error>
#include <vector>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <editor/panels/asset_metadata.hpp>

namespace editor {

auto asset_watcher::update(editor_state& state) -> void {
  const auto now = std::chrono::steady_clock::now();

  if (_has_snapshot && now - _last_scan < _interval) {
    return;
  }

  _last_scan = now;

  auto current = _scan();

  // The first scan only records: startup's import_directory already registered everything on disk.
  if (!_has_snapshot) {
    _files = std::move(current);
    _has_snapshot = true;
    return;
  }

  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  auto changed_anything = false;

  // Removals first: a file moved outside the editor together with its .meta must release its uuid before the new path claims it.
  for (const auto& [path, write_time] : _files) {
    if (current.contains(path)) {
      continue;
    }

    changed_anything = true;

    if (sbx::assets::asset_type_of(path) == sbx::assets::asset_type::script) {
      state.scripts_changed = true;
    }

    if (const auto id = assets_module.find(path); id != sbx::math::uuid::nil()) {
      assets_module.forget(id);
    }
  }

  for (const auto& [path_string, write_time] : current) {
    const auto previous = _files.find(path_string);

    if (previous != _files.end() && previous->second == write_time) {
      continue;
    }

    changed_anything = true;

    const auto path = std::filesystem::path{path_string};
    const auto type = sbx::assets::asset_type_of(path);

    if (type == sbx::assets::asset_type::script) {
      state.scripts_changed = true;
    }

    if (!sbx::assets::is_manifested(type)) {
      continue;
    }

    if (previous == _files.end()) {
      auto meta_path = path;
      meta_path += ".meta";

      if (!std::filesystem::exists(meta_path) && sbx::assets::has_import_settings(type)) {
        state.request_import(std::filesystem::relative(path, project.assets_directory()));
      } else {
        assets_module.import(path);
      }

      continue;
    }

    // The editor never writes .slang files, so a change is always an outside edit.
    if (type == sbx::assets::asset_type::shader) {
      if (const auto id = assets_module.find(path); id != sbx::math::uuid::nil()) {
        assets_module.reimport(id);
      }

      continue;
    }

    // ponytail: only sources with an importer hot-reload; editor-native YAML assets (materials, graphs, prefabs, scenes) are rewritten by the editor's own saves, so reloading them here would need self-write suppression first.
    if (sbx::assets::has_import_settings(type)) {
      if (const auto id = assets_module.find(path); id != sbx::math::uuid::nil()) {
        assets_module.reimport(id);
      }
    }
  }

  _files = std::move(current);

  if (changed_anything) {
    state.asset_browser_refresh_request = true;
  }
}

// ponytail: a full recursive stat every interval is O(files); switch to inotify/ReadDirectoryChangesW if large projects stall here.
auto asset_watcher::_scan() -> snapshot {
  auto& project = sbx::core::engine::project();

  auto files = snapshot{};
  auto error = std::error_code{};

  auto iterator = std::filesystem::recursive_directory_iterator{project.assets_directory(), std::filesystem::directory_options::skip_permission_denied, error};

  for (const auto end = std::filesystem::recursive_directory_iterator{}; !error && iterator != end; iterator.increment(error)) {
    const auto& entry = *iterator;

    // Per-entry errors (a file vanishing mid-scan) skip that entry; only iteration errors end the scan.
    auto entry_error = std::error_code{};
    const auto is_directory = entry.is_directory(entry_error);

    if (is_hidden_from_browser(entry.path().filename(), is_directory)) {
      if (is_directory) {
        iterator.disable_recursion_pending();
      }

      continue;
    }

    if (!is_directory) {
      const auto write_time = entry.last_write_time(entry_error);

      if (!entry_error) {
        files.emplace(entry.path().generic_string(), write_time);
      }
    }
  }

  return files;
}

} // namespace editor
