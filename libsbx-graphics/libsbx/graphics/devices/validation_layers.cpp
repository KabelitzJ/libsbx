// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/validation_layers.hpp>

#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/exception.hpp>

namespace sbx::graphics {

auto validation_layers::instance() -> std::vector<const char*> {
  auto required_layers = std::vector<const char*>{};

  if constexpr (utility::build_configuration_v == utility::build_configuration::debug) {
    // [NOTE] KAJ 2025-09-13 : Since I upgraded to VulkanSDK 1.4.321.0 the call to vkCreateInstance causes a segfault if I use this layer
    // I have no idea why, so for now I will disable it 
    // required_layers.push_back("VK_LAYER_KHRONOS_validation");

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
