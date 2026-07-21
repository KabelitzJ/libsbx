// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/devices/layers.hpp>

#include <vulkan/vulkan.h>

#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/target.hpp>
#include <libsbx/utility/exception.hpp>

namespace sbx::graphics {

auto layers::instance() -> std::vector<const char*> {
  auto required_layers = std::vector<const char*>{};

  if constexpr (utility::is_build_type_debug_v) {
    required_layers.push_back("VK_LAYER_KHRONOS_validation");

    auto available_layer_count = std::uint32_t{0};
    vkEnumerateInstanceLayerProperties(&available_layer_count, nullptr);

    auto available_layers = utility::make_vector<VkLayerProperties>(available_layer_count);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.data());

    for (const auto* required_layer : required_layers) {
      bool found = false;

      for (const auto& available_layer : available_layers) {
        if (std::strcmp(required_layer, available_layer.layerName) == 0) {
          found = true;
          break;
        }
      }

      if (!found) {
        throw utility::runtime_error{"Required layer not available: {}", std::string{required_layer}};
      }
    }
  }

  return required_layers;
}

} // namespace sbx::graphics
