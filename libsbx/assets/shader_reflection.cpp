// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/shader_reflection.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/assets/texture2d.hpp>

namespace sbx::assets {

using reflected_attribute = graphics::shader_compiler::reflected_attribute;
using reflected_field = graphics::shader_compiler::reflected_field;

auto find_attribute(const reflected_field& field, std::string_view name) -> const reflected_attribute* {
  const auto entry = std::ranges::find(field.attributes, name, &reflected_attribute::name);
  return entry != field.attributes.end() ? &*entry : nullptr;
}

auto string_argument(const reflected_attribute& attribute, std::size_t index) -> std::optional<std::string> {
  if (index >= attribute.arguments.size()) {
    return std::nullopt;
  }

  const auto* value = std::get_if<std::string>(&attribute.arguments[index]);
  return value != nullptr ? std::optional{*value} : std::nullopt;
}

auto float_argument(const reflected_attribute& attribute, std::size_t index) -> std::float_t {
  if (index >= attribute.arguments.size()) {
    return 0.0f;
  }

  const auto* value = std::get_if<std::float_t>(&attribute.arguments[index]);
  return value != nullptr ? *value : 0.0f;
}

// Reference and display name from [Property(reference, name)], else "_<field>" and the field name.
auto make_property(const reflected_field& field, std::uint32_t id) -> shader_property {
  auto property = shader_property{.id = id, .name = field.name, .reference = "_" + field.name};

  if (const auto* attribute = find_attribute(field, "Property")) {
    property.reference = string_argument(*attribute, 0u).value_or(property.reference);
    property.name = string_argument(*attribute, 1u).value_or(property.name);
  }

  property.is_hdr = find_attribute(field, "Hdr") != nullptr;
  property.is_linear = find_attribute(field, "Linear") != nullptr;

  return property;
}

auto value_property_type(const reflected_field& field) -> std::optional<shader_property_type> {
  if (field.type != reflected_field::kind::float_vector) {
    return std::nullopt;
  }

  switch (field.component_count) {
    case 1u: return shader_property_type::float_value;
    case 2u: return shader_property_type::vector2_value;
    case 3u: return shader_property_type::vector3_value;
    case 4u: return find_attribute(field, "Color") != nullptr ? shader_property_type::color_value : shader_property_type::vector4_value;
    default: return std::nullopt;
  }
}

// [Default(x, y, z, w)] narrowed to the property's type; zero without one.
auto default_value(const reflected_field& field, shader_property_type type) -> shader_property_value {
  const auto* attribute = find_attribute(field, "Default");

  const auto component = [&](std::size_t index) {
    return attribute != nullptr ? float_argument(*attribute, index) : 0.0f;
  };

  switch (type) {
    case shader_property_type::float_value: return component(0u);
    case shader_property_type::vector2_value: return math::vector2{component(0u), component(1u)};
    case shader_property_type::vector3_value: return math::vector3{component(0u), component(1u), component(2u)};
    case shader_property_type::vector4_value: return math::vector4{component(0u), component(1u), component(2u), component(3u)};
    case shader_property_type::color_value: return math::color{component(0u), component(1u), component(2u), component(3u)};
    case shader_property_type::texture_value: return texture2d_handle{};
  }

  return std::monostate{};
}

// Float `index` of the flat generic_params view, as a Slang expression.
auto param_component(std::uint32_t index) -> std::string {
  static constexpr auto swizzle = std::string_view{"xyzw"};
  return fmt::format("material.generic_params[{}].{}", index / 4u, swizzle[index % 4u]);
}

auto value_loader(std::uint32_t slot, std::uint32_t component_count) -> std::string {
  if (component_count == 1u) {
    return param_component(slot);
  }

  auto components = std::vector<std::string>{};

  for (auto component = 0u; component < component_count; ++component) {
    components.push_back(param_component(slot + component));
  }

  return fmt::format("float{}({})", component_count, fmt::join(components, ", "));
}

auto shader_properties_from_reflection(const graphics::shader_compiler::module_reflection& reflection) -> std::expected<reflected_properties, std::string> {
  const auto entry = reflection.structs.find("properties");

  if (entry == reflection.structs.end()) {
    return reflected_properties{};
  }

  auto result = reflected_properties{};
  auto loader = std::string{"properties load_properties(material_data material) {\n  properties result;\n"};

  auto param_offset = std::uint32_t{0u};
  auto texture_slot = std::uint32_t{0u};

  for (const auto& field : entry->second) {
    auto property = make_property(field, static_cast<std::uint32_t>(result.properties.size()) + 1u);

    if (field.type == reflected_field::kind::texture2d || field.type == reflected_field::kind::texture2d_array) {
      property.type = shader_property_type::texture_value;
      property.slot = texture_slot++;
      property.value = texture2d_handle{};
      property.is_texture_array = field.type == reflected_field::kind::texture2d_array;

      loader += fmt::format("  result.{} = {}[material.generic_textures[{}]];\n", field.name, property.is_texture_array ? "texture_arrays" : "textures", property.slot);
    } else if (const auto type = value_property_type(field)) {
      property.type = *type;
      property.slot = param_offset;
      property.value = default_value(field, *type);

      if (const auto* range = find_attribute(field, "Range"); range != nullptr && *type == shader_property_type::float_value) {
        property.float_mode = shader_property_float_mode::slider;
        property.slider_range = math::vector2{float_argument(*range, 0u), float_argument(*range, 1u)};
      }

      loader += fmt::format("  result.{} = {};\n", field.name, value_loader(param_offset, field.component_count));
      param_offset += field.component_count;
    } else {
      return std::unexpected{fmt::format("properties.{} is a {}; only float, float2, float3, float4, Texture2D and Texture2DArray are supported", field.name, field.type_name)};
    }

    result.properties.push_back(std::move(property));
  }

  if (param_offset > material_max_param_components) {
    return std::unexpected{fmt::format("properties takes {} floats; a material has {}", param_offset, material_max_param_components)};
  }

  if (texture_slot > material_max_textures) {
    return std::unexpected{fmt::format("properties has {} textures; a material has {} texture slots", texture_slot, material_max_textures)};
  }

  for (auto index = std::size_t{0u}; index < result.properties.size(); ++index) {
    const auto duplicate = std::ranges::find(result.properties.begin() + static_cast<std::ptrdiff_t>(index) + 1, result.properties.end(), result.properties[index].reference, &shader_property::reference);

    if (duplicate != result.properties.end()) {
      return std::unexpected{fmt::format("two properties use the reference '{}'", result.properties[index].reference)};
    }
  }

  loader += "  return result;\n}\n";
  result.material_loader = std::move(loader);

  return result;
}

auto reflect_shader(const std::filesystem::path& path) -> shader_reflection_result {
  static constexpr auto entry_point_names = std::array<std::string_view, 5u>{"vertex_main", "fragment_main", "depth_vertex_main", "depth_fragment_main", "compute_main"};
  static constexpr auto struct_names = std::array<std::string_view, 1u>{"properties"};

  auto& shader_compiler = core::engine::get_module<graphics::graphics_module>().shader_compiler();

  auto result = shader_reflection_result{};

  try {
    const auto reflection = shader_compiler.reflect_module(path, entry_point_names, struct_names);

    for (const auto& dependency : reflection.dependencies) {
      auto error = std::error_code{};
      const auto normalized = std::filesystem::weakly_canonical(dependency, error);
      result.dependencies.push_back(error ? dependency.lexically_normal() : normalized);
    }

    const auto has = [&](std::string_view name) {
      return std::ranges::find(reflection.entry_points, name) != reflection.entry_points.end();
    };

    auto properties = shader_properties_from_reflection(reflection);

    if (!properties) {
      utility::logger<"assets">::warn("Shader '{}': {}", path.generic_string(), properties.error());
      result.error = std::move(properties.error());
      return result;
    }

    result.kind = (has("vertex_main") && has("fragment_main")) ? shader_kind::surface : has("compute_main") ? shader_kind::compute : shader_kind::library;
    result.has_depth_entry_points = has("depth_vertex_main") && has("depth_fragment_main");
    result.properties = std::move(properties->properties);
    result.material_loader = std::move(properties->material_loader);
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Shader '{}' doesn't parse: {}", path.generic_string(), exception.what());
    result.error = exception.what();
  }

  return result;
}

} // namespace sbx::assets
