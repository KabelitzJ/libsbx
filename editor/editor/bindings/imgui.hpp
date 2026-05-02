// SPDX-License-Identifier: MIT
#ifndef EDITOR_IMGUI_BINDINGS_IMGUI_HPP_
#define EDITOR_IMGUI_BINDINGS_IMGUI_HPP_

#include <imgui.h>
#include <imgui_internal.h>
#include <imnodes.h>
#include <implot.h>
#include <ImGuizmo.h>

static_assert(sizeof(ImWchar) == 4, "IMGUI_USE_WCHAR32 not active");

#include <editor/bindings/1.91.8-docking/imgui_impl_glfw.h>
#include <editor/bindings/1.91.8-docking/imgui_impl_vulkan.h>

#include <editor/bindings/fonts/material_design_icons.hpp>

#endif // EDITOR_IMGUI_BINDINGS_IMGUI_HPP_
