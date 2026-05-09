// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_PROVINCE_MAP_HPP_
#define DEMO_TERRAIN_PROVINCE_MAP_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <demo/terrain/terrain_types.hpp>

namespace demo {

class province_map {

public:

  using province_id_t = std::uint32_t;

  struct province_record {
    landform terrain{landform::plains};
    std::uint8_t land_use{0u};
    world_coordinates centroid{};
    std::float_t elevation{0.0f};
    std::float_t moisture{0.0f};
  }; // struct province_record

  province_map(const std::filesystem::path& ids_path, const std::filesystem::path& metadata_path, std::uint32_t width, std::uint32_t height, std::float_t cell_size)
  : _width{width},
    _height{height},
    _cell_size{cell_size} {
    _load_ids(ids_path);
    _load_metadata(metadata_path);
  }

  auto province_at(const world_coordinates& coordinates) const -> province_id_t {
    auto pixel_x = (coordinates.x + _offset_x()) / _cell_size;
    auto pixel_y = (coordinates.z + _offset_z()) / _cell_size;

    auto px = static_cast<std::int32_t>(std::floor(pixel_x));
    auto py = static_cast<std::int32_t>(std::floor(pixel_y));

    return _id_at_pixel_clamped(px, py);
  }

  auto landform_at(const world_coordinates& coordinates) const -> landform {
    return landform_of(province_at(coordinates));
  }

  auto landform_of(province_id_t id) const -> landform {
    if (id >= _records.size()) {
      return landform::plains;
    }

    return _records[id].terrain;
  }

  auto centroid_of(province_id_t id) const -> world_coordinates {
    if (id >= _records.size()) {
      return world_coordinates{};
    }

    return _records[id].centroid;
  }

  auto record_of(province_id_t id) const -> const province_record& {
    return _records[id];
  }

  auto landform_at_pixel(std::uint32_t pixel_x, std::uint32_t pixel_y) const -> landform {
    return _records[_id_at_pixel(pixel_x, pixel_y)].terrain;
  }

  auto province_at_pixel(std::uint32_t pixel_x, std::uint32_t pixel_y) const -> province_id_t {
    return _id_at_pixel(pixel_x, pixel_y);
  }

  auto province_count() const -> std::uint32_t {
    return static_cast<std::uint32_t>(_records.size());
  }

  auto width() const -> std::uint32_t {
    return _width;
  }

  auto height() const -> std::uint32_t {
    return _height;
  }

  auto records() const -> const std::vector<province_record>& {
    return _records;
  }

  auto ids_data() const -> const province_id_t* {
    return _ids.data();
  }

  auto ids_data_size_bytes() const -> std::size_t {
    return _ids.size() * sizeof(province_id_t);
  }

private:

  auto _offset_x() const -> std::float_t {
    return static_cast<std::float_t>(_width - 1u) * _cell_size * 0.5f;
  }

  auto _offset_z() const -> std::float_t {
    return static_cast<std::float_t>(_height - 1u) * _cell_size * 0.5f;
  }

  auto _id_at_pixel(std::uint32_t pixel_x, std::uint32_t pixel_y) const -> province_id_t {
    return _ids[static_cast<std::size_t>(pixel_y) * _width + pixel_x];
  }

  auto _id_at_pixel_clamped(std::int32_t pixel_x, std::int32_t pixel_y) const -> province_id_t {
    auto px = static_cast<std::uint32_t>(std::clamp(pixel_x, 0, static_cast<std::int32_t>(_width - 1)));
    auto py = static_cast<std::uint32_t>(std::clamp(pixel_y, 0, static_cast<std::int32_t>(_height - 1)));

    return _ids[static_cast<std::size_t>(py) * _width + px];
  }

  auto _load_ids(const std::filesystem::path& path) -> void {
    auto file = std::ifstream{path, std::ios::binary};

    if (!file) {
      throw std::runtime_error{"failed to open province ids: " + path.string()};
    }

    auto count = static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height);

    _ids.resize(count);

    file.read(reinterpret_cast<char*>(_ids.data()), static_cast<std::streamsize>(count * sizeof(province_id_t)));

    if (!file) {
      throw std::runtime_error{"failed to read province ids: " + path.string()};
    }
  }

  auto _load_metadata(const std::filesystem::path& path) -> void {
    auto root = YAML::LoadFile(path.string());

    auto provinces = root["provinces"];

    if (!provinces) {
      throw std::runtime_error{"missing 'provinces' in metadata: " + path.string()};
    }

    _records.reserve(provinces.size());

    for (auto i = std::size_t{0}; i < provinces.size(); ++i) {
      auto province = provinces[i];

      auto record = province_record{};

      record.terrain = static_cast<landform>(province["terrain"].as<std::uint32_t>());
      record.land_use = province["land_use"].as<std::uint8_t>();

      auto centroid_pixels = province["centroid"];

      auto centroid_x_pixels = centroid_pixels[0].as<std::float_t>();
      auto centroid_y_pixels = centroid_pixels[1].as<std::float_t>();

      record.centroid.x = centroid_x_pixels * _cell_size - _offset_x();
      record.centroid.z = centroid_y_pixels * _cell_size - _offset_z();

      record.elevation = province["elevation"].as<std::float_t>();
      record.moisture = province["moisture"].as<std::float_t>();

      _records.push_back(record);
    }
  }

  std::uint32_t _width;
  std::uint32_t _height;
  std::float_t _cell_size;
  std::vector<province_id_t> _ids;
  std::vector<province_record> _records;

}; // class province_map

} // namespace demo

#endif // DEMO_TERRAIN_PROVINCE_MAP_HPP_
