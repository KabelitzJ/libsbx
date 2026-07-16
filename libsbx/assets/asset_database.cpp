// SPDX-License-Identifier: MIT
#include <libsbx/assets/asset_database.hpp>

#include <libsbx/utility/exception.hpp>

namespace sbx::assets {

auto asset_database::insert(asset_record record) -> asset_record& {
  if (record.id == math::uuid::nil()) {
    throw utility::runtime_error{"Refusing to insert asset_record with nil UUID"};
  }

  if (auto entry = _records.find(record.id); entry != _records.end()) {
    if (!entry->second.source.empty() && (entry->second.source != record.source || entry->second.sub_id != record.sub_id)) {
      _erase_path_entry(entry->second.source, entry->second.sub_id);
    }

    entry->second = std::move(record);

    if (!entry->second.source.empty()) {
      _by_path[_key(entry->second.source, entry->second.sub_id)] = entry->second.id;
    }

    return entry->second;
  }

  const auto id = record.id;
  const auto source = record.source;
  const auto sub_id = record.sub_id;

  auto [entry, inserted] = _records.emplace(id, std::move(record));

  if (!source.empty()) {
    _by_path[_key(source, sub_id)] = id;
  }

  return entry->second;
}

auto asset_database::erase(const math::uuid& id) -> bool {
  auto entry = _records.find(id);

  if (entry == _records.end()) {
    return false;
  }

  if (!entry->second.source.empty()) {
    _erase_path_entry(entry->second.source, entry->second.sub_id);
  }

  _records.erase(entry);

  return true;
}

auto asset_database::contains(const math::uuid& id) const -> bool {
  return _records.contains(id);
}

auto asset_database::contains_path(const std::filesystem::path& source, std::string_view sub_id) const -> bool {
  return _by_path.contains(_key(source, sub_id));
}

auto asset_database::get(const math::uuid& id) -> asset_record& {
  auto entry = _records.find(id);

  if (entry == _records.end()) {
    throw utility::runtime_error{"No asset_record for UUID '{}'", id};
  }

  return entry->second;
}

auto asset_database::get(const math::uuid& id) const -> const asset_record& {
  const auto entry = _records.find(id);

  if (entry == _records.end()) {
    throw utility::runtime_error{"No asset_record for UUID '{}'", id};
  }

  return entry->second;
}

auto asset_database::try_get(const math::uuid& id) -> memory::observer_ptr<asset_record> {
  if (auto entry = _records.find(id); entry != _records.end()) {
    return &entry->second;
  }

  return nullptr;
}

auto asset_database::try_get(const math::uuid& id) const -> memory::observer_ptr<const asset_record> {
  if (const auto entry = _records.find(id); entry != _records.end()) {
    return &entry->second;
  }

  return nullptr;
}

auto asset_database::resolve(const std::filesystem::path& source, std::string_view sub_id) const -> math::uuid {
  if (const auto entry = _by_path.find(_key(source, sub_id)); entry != _by_path.end()) {
    return entry->second;
  }

  return math::uuid::nil();
}

auto asset_database::rebind_path(const math::uuid& id, const std::filesystem::path& source) -> void {
  auto& record = get(id);

  if (!record.source.empty()) {
    _erase_path_entry(record.source, record.sub_id);
  }

  record.source = std::move(source);

  if (!record.source.empty()) {
    _by_path[_key(record.source, record.sub_id)] = id;
  }
}

auto asset_database::acquire(const math::uuid& id) -> std::uint32_t {
  auto& record = get(id);

  return ++record.reference_count;
}

auto asset_database::release(const math::uuid& id) -> std::uint32_t {
  auto& record = get(id);

  if (record.reference_count == 0u) {
    return 0u;
  }

  return --record.reference_count;
}

auto asset_database::bump_generation(const math::uuid& id) -> std::uint32_t {
  auto& record = get(id);

  return ++record.generation;
}

auto asset_database::clear() -> void {
  _records.clear();
  _by_path.clear();
}

auto asset_database::_key(const std::filesystem::path& source, std::string_view sub_id) -> std::string {
  auto key = source.generic_string();

  if (!sub_id.empty()) {
    key += '#';
    key += sub_id;
  }

  return key;
}

auto asset_database::_erase_path_entry(const std::filesystem::path& source, std::string_view sub_id) -> void {
  _by_path.erase(_key(source, sub_id));
}

} // namespace sbx::assets
