// SPDX-License-Identifier: MIT
#include <editor/panels/texture_cache.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <editor/bindings/imgui.hpp>

namespace editor {

texture_cache::~texture_cache() {
  release_all();
}

auto texture_cache::descriptor_for(const sbx::graphics::image2d_handle& handle) -> VkDescriptorSet {
  if (!handle.is_valid()) {
    return VK_NULL_HANDLE;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  const auto& image = graphics_module.get_resource<sbx::graphics::image2d>(handle);

  const auto current_view = image.view();

  if (current_view == VK_NULL_HANDLE) {
    return VK_NULL_HANDLE;
  }

  auto entry_it = _entries.find(handle);

  if (entry_it != _entries.end()) {
    if (entry_it->second.cached_view == current_view) {
      return entry_it->second.descriptor_set;
    }

    if (entry_it->second.descriptor_set != VK_NULL_HANDLE) {
      ImGui_ImplVulkan_RemoveTexture(entry_it->second.descriptor_set);
    }

    entry_it->second.descriptor_set = ImGui_ImplVulkan_AddTexture(image.sampler(), current_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    entry_it->second.cached_view = current_view;

    return entry_it->second.descriptor_set;
  }

  auto descriptor_set = ImGui_ImplVulkan_AddTexture(image.sampler(), current_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  _entries.emplace(handle, entry{descriptor_set, current_view});

  return descriptor_set;
}

auto texture_cache::release_all() -> void {
  if (ImGui::GetCurrentContext() == nullptr) {
    _entries.clear();
    return;
  }

  for (auto& [handle, item] : _entries) {
    if (item.descriptor_set != VK_NULL_HANDLE) {
      ImGui_ImplVulkan_RemoveTexture(item.descriptor_set);
    }
  }

  _entries.clear();
}

} // namespace editor
