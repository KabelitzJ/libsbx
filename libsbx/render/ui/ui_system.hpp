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
 * @brief Owns ImGui end to end: context, the vendored GLFW + Vulkan backends (moved here from
 * editor/, see libsbx/render/ui/backends), fonts, the registered ui_layer list, and the two halves
 * of a frame's UI — build_frame() (main thread) and render() (render thread, consuming the deep
 * copy build_frame() produced — see ui_draw_data).
 *
 * Owned by ui_module (see ui_module.hpp), which implements ui_renderer and registers with
 * presentation_module — ui_system itself knows nothing about core::module or presentation_module
 * at all, only ImGui.
 */
class ui_system final : public utility::noncopyable {

public:

  ui_system();

  ~ui_system();

  /**
   * @brief Registers a layer this ui_system owns outright — mirrors render_graph::add_pass<Pass>.
   * build() order is registration order, across both this and the observer_ptr overload below.
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
   * @brief Registers a layer owned elsewhere (e.g. editor_module, itself owned by the engine's
   * module system) — pair with remove_layer before @p layer is destroyed.
   */
  auto add_layer(memory::observer_ptr<ui_layer> layer) -> void;

  auto remove_layer(memory::observer_ptr<ui_layer> layer) -> void;

  /**
   * @brief Loads a font from @p path at @p size_pixels. For anything beyond that (merged icon
   * fonts, custom glyph ranges, ...) use fonts() directly, same as you would with any ImFontAtlas.
   */
  auto add_font(const std::filesystem::path& path, std::float_t size_pixels) -> ImFont*;

  /**
   * @brief Loads the engine's built-in font (Roboto Regular + Material Design Icons glyph range) as
   * io.FontDefault, at @p size_pixels. Compiled into libsbx, not read from disk; defined in its own
   * translation unit (fonts/embedded_fonts.cpp) so apps that never call this don't pay for the
   * ~1.5MB of font data. Safe to call more than once — each call adds another ImFont and becomes
   * the new default.
   */
  auto add_default_fonts(std::float_t size_pixels = 16.0f) -> void;

  [[nodiscard]] auto fonts() noexcept -> ImFontAtlas* {
    return ImGui::GetIO().Fonts;
  }

  /**
   * @brief Applies the engine's default ImGui style (dark Catppuccin-Mocha, srgb-to-linear
   * corrected). Opt-in, like add_default_fonts() — call once, any time after construction, before
   * the first frame you want it visible in.
   */
  auto apply_default_style() -> void;

  /**
   * @brief Main-thread build step, called from ui_module::build_frame (in turn called from
   * presentation_module::render): collects textures retired by previous frames, primes the
   * backends, ImGui::NewFrame(), every registered layer's build() in order, ImGui::Render(), then
   * deep-copies the result — see ui_draw_data.
   */
  [[nodiscard]] auto build_frame() -> ui_draw_data;

  /**
   * @brief Render-thread render step, called from ui_module::render (in turn called from
   * presentation_module's kicked work, right after the compositor step). A no-op if
   * !data.is_valid(). Draws onto the swapchain with VK_ATTACHMENT_LOAD_OP_LOAD — the compositor
   * (or presentation_module's own clear_swapchain fallback) is responsible for giving it a
   * defined background first.
   */
  auto render(graphics::command_buffer& command_buffer, math::vector2u extent, const ui_draw_data& data) -> void;

  /**
   * @brief Memoized ImGui_ImplVulkan_AddTexture, for e.g. sampling a render target inside an
   * ImGui::Image() call. Call every frame with the *current* view/sampler; a texture is only
   * (re)registered when the pair actually changes (e.g. a render target resize), and the
   * descriptor set it replaces is retired once the GPU has caught up (see _collect_pending_textures).
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
