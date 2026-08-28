// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_UI_DRAW_DATA_HPP_
#define LIBSBX_RENDER_UI_UI_DRAW_DATA_HPP_

#include <cstdint>
#include <vector>

#include <imgui.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/vector2.hpp>

namespace sbx::render {

/**
 * @brief Main-thread-owned deep copy of one frame's ImGui draw output.
 *
 * ImGui::GetDrawData() returns pointers into the context's own per-window ImDrawList buffers,
 * which the very next ImGui::NewFrame() (built while ui_system::render may still be reading the
 * previous frame's data on the render thread) reuses. render_pass implementations never reach into
 * ImGui's global context directly, same as they never reach into the ECS (see render_module's doc
 * comment) — they only ever read render_context::packet. This is the piece of the packet that
 * carries UI: built once per frame, on the main thread, from ImGui::GetDrawData() (see
 * ui_system::build_frame), using ImDrawList::CloneOutput() — Dear ImGui's own documented mechanism
 * for consuming one frame's draw data from a thread other than the one that built it.
 *
 * Move-only: owns the cloned ImDrawList objects outright.
 */
class ui_draw_data final : public utility::noncopyable {

public:

  ui_draw_data() = default;

  /** @brief Deep-copies @p source. Leaves *this invalid (is_valid() == false) if @p source is null, !Valid, or empty. */
  explicit ui_draw_data(const ImDrawData* source);

  ui_draw_data(ui_draw_data&& other) noexcept;

  auto operator=(ui_draw_data&& other) noexcept -> ui_draw_data&;

  ~ui_draw_data();

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _is_valid;
  }

  /** @brief Owning clones, one per source ImDrawList — see ImDrawList::CloneOutput(). */
  [[nodiscard]] auto draw_lists() const noexcept -> const std::vector<ImDrawList*>& {
    return _draw_lists;
  }

  [[nodiscard]] auto display_pos() const noexcept -> const math::vector2& {
    return _display_pos;
  }

  [[nodiscard]] auto display_size() const noexcept -> const math::vector2& {
    return _display_size;
  }

  [[nodiscard]] auto framebuffer_scale() const noexcept -> const math::vector2& {
    return _framebuffer_scale;
  }

  /** @brief Sum of every list's VtxBuffer/IdxBuffer size — ImGui_ImplVulkan_RenderDrawData needs these to size its own upload buffers, it doesn't recompute them from the lists. */
  [[nodiscard]] auto total_vertex_count() const noexcept -> std::int32_t {
    return _total_vertex_count;
  }

  [[nodiscard]] auto total_index_count() const noexcept -> std::int32_t {
    return _total_index_count;
  }

  /**
   * @brief The texture update-request list ImGui_ImplVulkan_RenderDrawData needs to process the
   * font atlas's create/update requests (ImGuiBackendFlags_RendererHasTextures). A pointer shared
   * across every frame — ImGui::GetPlatformIO().Textures, owned by the context, not reallocated
   * per frame — so this is forwarded, never deep-copied.
   */
  [[nodiscard]] auto textures() const noexcept -> ImVector<ImTextureData*>* {
    return _textures;
  }

private:

  auto _release() noexcept -> void;

  std::vector<ImDrawList*> _draw_lists{};

  math::vector2 _display_pos{0.0f, 0.0f};
  math::vector2 _display_size{0.0f, 0.0f};
  math::vector2 _framebuffer_scale{1.0f, 1.0f};

  std::int32_t _total_vertex_count{0};
  std::int32_t _total_index_count{0};

  ImVector<ImTextureData*>* _textures{nullptr};

  bool _is_valid{false};

}; // class ui_draw_data

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_UI_DRAW_DATA_HPP_
