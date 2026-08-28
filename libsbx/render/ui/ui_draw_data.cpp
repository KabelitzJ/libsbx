// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/ui/ui_draw_data.hpp>

#include <utility>

namespace sbx::render {

ui_draw_data::ui_draw_data(const ImDrawData* source) {
  if (source == nullptr || !source->Valid || source->CmdListsCount <= 0) {
    return;
  }

  _draw_lists.reserve(static_cast<std::size_t>(source->CmdListsCount));

  for (auto index = 0; index < source->CmdListsCount; ++index) {
    auto* clone = source->CmdLists[index]->CloneOutput();

    _total_vertex_count += clone->VtxBuffer.Size;
    _total_index_count += clone->IdxBuffer.Size;

    _draw_lists.push_back(clone);
  }

  _display_pos = math::vector2{source->DisplayPos.x, source->DisplayPos.y};
  _display_size = math::vector2{source->DisplaySize.x, source->DisplaySize.y};
  _framebuffer_scale = math::vector2{source->FramebufferScale.x, source->FramebufferScale.y};
  _textures = source->Textures;

  _is_valid = true;
}

ui_draw_data::ui_draw_data(ui_draw_data&& other) noexcept
: _draw_lists{std::move(other._draw_lists)},
  _display_pos{other._display_pos},
  _display_size{other._display_size},
  _framebuffer_scale{other._framebuffer_scale},
  _total_vertex_count{other._total_vertex_count},
  _total_index_count{other._total_index_count},
  _textures{other._textures},
  _is_valid{other._is_valid} {
  other._total_vertex_count = 0;
  other._total_index_count = 0;
  other._textures = nullptr;
  other._is_valid = false;
}

auto ui_draw_data::operator=(ui_draw_data&& other) noexcept -> ui_draw_data& {
  if (this != &other) {
    _release();

    _draw_lists = std::move(other._draw_lists);
    _display_pos = other._display_pos;
    _display_size = other._display_size;
    _framebuffer_scale = other._framebuffer_scale;
    _total_vertex_count = other._total_vertex_count;
    _total_index_count = other._total_index_count;
    _textures = other._textures;
    _is_valid = other._is_valid;

    other._total_vertex_count = 0;
    other._total_index_count = 0;
    other._textures = nullptr;
    other._is_valid = false;
  }

  return *this;
}

ui_draw_data::~ui_draw_data() {
  _release();
}

auto ui_draw_data::_release() noexcept -> void {
  for (auto* list : _draw_lists) {
    IM_DELETE(list);
  }

  _draw_lists.clear();
}

} // namespace sbx::render
