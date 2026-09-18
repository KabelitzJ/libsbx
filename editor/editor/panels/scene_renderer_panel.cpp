// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/scene_renderer_panel.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

namespace editor {

// Same helpers as statistics_panel.cpp -- kept local rather than shared since there are only two
// consumers so far; extract a common widget if a third panel ends up needing the same row shape.
static auto _table_row(std::initializer_list<std::string> columns) -> void {
  ImGui::TableNextRow();

  auto column_index = int{0};

  for (const auto& text : columns) {
    ImGui::TableSetColumnIndex(column_index++);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text.c_str());
  }
}

inline constexpr auto table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;

static auto _setup_label_value_columns() -> void {
  ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 160.0f);
  ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
}

auto scene_renderer_panel::draw(editor_state& state) -> void {
  static_cast<void>(state);

  if (!is_open) {
    return;
  }

  if (!ImGui::Begin(window_name, &is_open)) {
    ImGui::End();

    return;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
  auto& frame_context = graphics_module.frame_context();

  const auto extent = scene_renderer_module.target_extent();

  if (ImGui::BeginTable("##scene_renderer_header_table", 2, table_flags)) {
    _setup_label_value_columns();

    _table_row({"Viewport size", fmt::format("{}x{}", extent.x(), extent.y())});

    if (frame_context.is_initialized()) {
      const auto present_mode = frame_context.swapchain().present_mode();
      _table_row({"Present mode", present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "FIFO"});
    } else {
      _table_row({"Present mode", "not initialized yet"});
    }

    _table_row({"Total GPU time", fmt::format("{:.3f} ms", scene_renderer_module.total_gpu_time_ms())});

    ImGui::EndTable();
  }

  ImGui::SeparatorText("Per-pass GPU time");
  ImGui::TextDisabled("Always max_frames_in_flight frames stale (query-pool readback).");

  // Copied rather than sorted in place -- pass_timings() is render_graph's own live buffer, read
  // fresh every draw() call, and reordering it would desync it from the graph's declaration order
  // every other reader (and the next frame's readback-by-index) relies on.
  const auto pass_timings_span = scene_renderer_module.pass_timings();
  auto timings = std::vector<sbx::render::pass_gpu_timing>{pass_timings_span.begin(), pass_timings_span.end()};

  if (ImGui::BeginTable("##pass_timings_table", 2, table_flags | ImGuiTableFlags_Sortable)) {
    ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_DefaultSort);
    ImGui::TableSetupColumn("Time");
    ImGui::TableHeadersRow();

    if (auto* sort_specs = ImGui::TableGetSortSpecs(); sort_specs && sort_specs->SpecsCount > 0) {
      const auto& spec = sort_specs->Specs[0];
      const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

      if (spec.ColumnIndex == 0) {
        std::ranges::sort(timings, [ascending](const auto& lhs, const auto& rhs) { return ascending ? lhs.name < rhs.name : lhs.name > rhs.name; });
      } else {
        std::ranges::sort(timings, [ascending](const auto& lhs, const auto& rhs) { return ascending ? lhs.milliseconds < rhs.milliseconds : lhs.milliseconds > rhs.milliseconds; });
      }
    }

    for (const auto& timing : timings) {
      _table_row({std::string{timing.name}, fmt::format("{:.3f} ms", timing.milliseconds)});
    }

    ImGui::EndTable();
  }

  ImGui::SeparatorText("Pipeline statistics");

  const auto& stats = scene_renderer_module.pipeline_stats();

  if (ImGui::BeginTable("##pipeline_stats_table", 2, table_flags)) {
    _setup_label_value_columns();

    _table_row({"Input assembly vertices", fmt::format("{}", stats.input_assembly_vertices)});
    _table_row({"Input assembly primitives", fmt::format("{}", stats.input_assembly_primitives)});
    _table_row({"Vertex shader invocations", fmt::format("{}", stats.vertex_shader_invocations)});
    _table_row({"Clipping invocations", fmt::format("{}", stats.clipping_invocations)});
    _table_row({"Clipping primitives", fmt::format("{}", stats.clipping_primitives)});
    _table_row({"Fragment shader invocations", fmt::format("{}", stats.fragment_shader_invocations)});

    ImGui::EndTable();
  }

  ImGui::End();
}

} // namespace editor
