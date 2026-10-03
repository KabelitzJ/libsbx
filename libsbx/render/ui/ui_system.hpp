// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_UI_SYSTEM_HPP_
#define LIBSBX_RENDER_UI_UI_SYSTEM_HPP_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h>

#include <imgui.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/ui/ui_layer.hpp>
#include <libsbx/render/ui/ui_draw_data.hpp>

namespace sbx::render {

/**
 * @brief Owns ImGui end to end: context, GLFW and Vulkan backends, fonts and the registered layers.
 *
 * build_frame() runs on the main thread and render() on the render thread, consuming build_frame()'s deep copy. Knows nothing about the module system.
 */
class ui_system final : public utility::noncopyable {

public:

  ui_system();

  ~ui_system();

  /**
   * @brief Registers a layer this ui_system owns. Layers build in registration order across both add_layer overloads.
   *
   * @tparam Layer The layer type.
   * @tparam Args The constructor argument types.
   *
   * @param args The layer's constructor arguments.
   *
   * @return The new layer.
   */
  template<typename Layer, typename... Args>
  requires (std::is_base_of_v<ui_layer, Layer> && std::is_constructible_v<Layer, Args...>)
  auto add_layer(Args&&... args) -> Layer& {
    auto owned = std::make_unique<Layer>(std::forward<Args>(args)...);
    auto& ref = *owned;

    _layers.push_back(memory::observer_ptr<ui_layer>{&ref});
    _owned_layers.push_back(std::move(owned));

    return ref;
  }

  /**
   * @brief Registers a layer owned elsewhere; call remove_layer before it is destroyed.
   *
   * @param layer The layer to register.
   */
  auto add_layer(memory::observer_ptr<ui_layer> layer) -> void;

  auto remove_layer(memory::observer_ptr<ui_layer> layer) -> void;

  /**
   * @brief Loads a font from @p path; use fonts() directly for merged icons or custom glyph ranges.
   *
   * @param path The font file.
   * @param size_pixels The font size.
   *
   * @return The loaded font.
   */
  auto add_font(const std::filesystem::path& path, std::float_t size_pixels) -> ImFont*;

  /**
   * @brief Loads the compiled-in Roboto + Material Design Icons font as the default. Each call adds another font and makes it the default.
   *
   * @param size_pixels The font size.
   */
  auto add_default_fonts(std::float_t size_pixels = 16.0f) -> void;

  [[nodiscard]] auto fonts() noexcept -> ImFontAtlas* {
    return ImGui::GetIO().Fonts;
  }

  /** @brief Applies the engine's default ImGui style (Catppuccin Mocha, sRGB-corrected). Opt-in; call once before the first frame it should show in. */
  auto apply_default_style() -> void;

  /**
   * @brief Builds the frame on the main thread: collects retired textures, runs every layer's build() and deep-copies the result.
   *
   * @return The frame's draw data.
   */
  [[nodiscard]] auto build_frame() -> ui_draw_data;

  /**
   * @brief Draws @p data onto the swapchain on the render thread, loading the existing contents. No-op if @p data is invalid.
   *
   * @param command_buffer The command buffer to record into.
   * @param extent The swapchain extent.
   * @param data The draw data from build_frame().
   */
  auto render(graphics::command_buffer& command_buffer, math::vector2u extent, const ui_draw_data& data) -> void;

  /**
   * @brief Memoized ImGui texture registration for sampling a view in ImGui::Image(). Call every frame; it re-registers only when the pair changes and retires the old set once the GPU is done.
   *
   * @param view The image view.
   * @param sampler The sampler.
   *
   * @return The ImGui texture id.
   */
  [[nodiscard]] auto texture_id(VkImageView view, VkSampler sampler) -> ImTextureID;

private:

  struct texture_entry {
    VkDescriptorSet descriptor_set;
    VkSampler sampler;
  }; // struct texture_entry

  auto _retire_texture(VkDescriptorSet descriptor_set) -> void;

  auto _collect_pending_textures() -> void;

  std::vector<std::unique_ptr<ui_layer>> _owned_layers{};
  std::vector<memory::observer_ptr<ui_layer>> _layers{};

  std::unordered_map<VkImageView, texture_entry> _textures{};
  std::vector<std::pair<VkDescriptorSet, std::uint64_t>> _pending_texture_frees{};

}; // class ui_system

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_UI_SYSTEM_HPP_
