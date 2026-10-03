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

/** @brief A deep copy of one frame's ImGui draw output: the next NewFrame() reuses ImGui's buffers while the render thread may still read them. Move-only. */
class ui_draw_data final : public utility::noncopyable {

public:

  ui_draw_data() = default;

  /**
   * @brief Deep-copies @p source.
   *
   * @param source The frame's draw data; null, invalid or empty leaves this invalid.
   */
  explicit ui_draw_data(const ImDrawData* source);

  ui_draw_data(ui_draw_data&& other) noexcept;

  auto operator=(ui_draw_data&& other) noexcept -> ui_draw_data&;

  ~ui_draw_data();

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return _is_valid;
  }

  /**
   * @brief Owning clones, one per source draw list.
   *
   * @return The draw lists.
   */
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

  /**
   * @brief Total vertex count across every list, which the Vulkan backend needs to size its upload buffers.
   *
   * @return The vertex count.
   */
  [[nodiscard]] auto total_vertex_count() const noexcept -> std::int32_t {
    return _total_vertex_count;
  }

  [[nodiscard]] auto total_index_count() const noexcept -> std::int32_t {
    return _total_index_count;
  }

  /**
   * @brief The context's shared texture update list (font atlas requests), forwarded rather than copied.
   *
   * @return The texture list.
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
