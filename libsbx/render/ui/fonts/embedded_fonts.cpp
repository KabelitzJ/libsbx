// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
//
// Deliberately its own translation unit — see ui_system::add_default_fonts's doc comment. Nothing
// in ui_system.cpp (or anywhere else always linked into every app) references anything defined
// here, so an app that never calls add_default_fonts() (e.g. demo) never pulls this .o, and hence
// never pulls the ~1.5MB of embedded font data below, into its binary at all.
#include <libsbx/render/ui/ui_system.hpp>

#include <array>

#include <imgui.h>

#include <libsbx/render/ui/fonts/generated/roboto_regular_ttf.hpp>
#include <libsbx/render/ui/fonts/generated/material_design_icons_ttf.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

namespace sbx::render {

auto ui_system::add_default_fonts(std::float_t size_pixels) -> void {
  auto& io = ImGui::GetIO();

  auto font_config = ImFontConfig{};
  font_config.FontDataOwnedByAtlas = false;

  auto* font = io.Fonts->AddFontFromMemoryTTF(
    const_cast<unsigned char*>(fonts::roboto_regular_ttf),
    static_cast<std::int32_t>(fonts::roboto_regular_ttf_size),
    size_pixels,
    &font_config
  );

  io.FontDefault = font;

  static constexpr auto icon_ranges = std::array<ImWchar, 3>{ICON_MIN_MDI, ICON_MAX_MDI, 0};

  auto icon_config = ImFontConfig{};
  icon_config.FontDataOwnedByAtlas = false;
  icon_config.MergeMode = true;
  icon_config.PixelSnapH = true;
  icon_config.GlyphMinAdvanceX = size_pixels;
  icon_config.GlyphOffset.y = 1.0f;

  io.Fonts->AddFontFromMemoryTTF(
    const_cast<unsigned char*>(fonts::material_design_icons_ttf),
    static_cast<std::int32_t>(fonts::material_design_icons_ttf_size),
    size_pixels,
    &icon_config,
    icon_ranges.data()
  );
}

} // namespace sbx::render
