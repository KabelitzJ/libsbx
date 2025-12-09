#include <libsbx/models/material.hpp>

#include <libsbx/utility/exception.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <nlohmann/json.hpp>

namespace sbx::models {

auto texture_slot_hash::operator()(const texture_slot& texture_slot) const noexcept -> std::size_t {
  auto hash = std::size_t{0};

  utility::hash_combine(hash, texture_slot.address_mode_u, texture_slot.address_mode_v, texture_slot.min_filter, texture_slot.mag_filter, texture_slot.anisotropy);
  
  return hash;
}

auto load_material(const std::filesystem::path& path) -> load_material_result {
  if (!std::filesystem::exists(path)) {
    throw utility::runtime_error{"Path does not exist: '{}", path.string()};
  }

  auto file = std::ifstream{path};

  if (!file.is_open()) {
    throw utility::runtime_error{"Path could not be opened: '{}", path.string()};
  }

  auto result = load_material_result{};

  auto definition = nlohmann::json::parse(file);

  if (definition.contains("name")) {
    result.name = definition["name"].get<std::string>();
  }

  return result;
}

} // namespace sbx::models