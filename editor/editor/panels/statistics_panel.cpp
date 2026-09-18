// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/statistics_panel.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <imgui.h>

#include <vk_mem_alloc.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/resources/resource_registry.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

#include <libsbx/utility/stats_registry.hpp>

#include <editor/memory_stats.hpp>

namespace editor {

static auto _format_bytes(std::uint64_t bytes) -> std::string {
  constexpr auto units = std::array<const char*, 4u>{"B", "KiB", "MiB", "GiB"};

  auto value = static_cast<std::double_t>(bytes);
  auto unit_index = std::size_t{0u};

  while (value >= 1024.0 && unit_index + 1u < units.size()) {
    value /= 1024.0;
    ++unit_index;
  }

  return fmt::format("{:.2f} {}", value, units[unit_index]);
}

// Signed variant for deltas -- a negative delta (freed more than allocated this frame) is common
// and useful to see, not an error case to clamp away.
static auto _format_bytes_delta(std::int64_t bytes) -> std::string {
  const auto sign = bytes < 0 ? "-" : "+";

  return fmt::format("{}{}", sign, _format_bytes(static_cast<std::uint64_t>(bytes < 0 ? -bytes : bytes)));
}

// A row of already-formatted cells in whatever table is currently open -- every _draw_*_tab table
// below is a plain label/value (or label/value/value) grid, so one helper covers all of them.
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

// A plain label/value table has no header row, but still needs its label column pinned to a
// readable width -- otherwise SizingStretchProp splits label/value 50/50, which looks odd once
// values get longer than labels (see inspector_asset_editors.cpp's own fixed-label-column table
// for the same reasoning).
static auto _setup_label_value_columns() -> void {
  ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 160.0f);
  ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
}

auto statistics_panel::draw(editor_state& state) -> void {
  static_cast<void>(state);

  ImGui::Begin(window_name);

  if (ImGui::BeginTabBar("##statistics_tabs")) {
    if (ImGui::BeginTabItem("Renderer")) {
      _draw_renderer_tab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Performance")) {
      _draw_performance_tab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Memory")) {
      _draw_memory_tab();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}

auto statistics_panel::_draw_renderer_tab() -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& physical_device = graphics_module.physical_device();
  const auto& properties = physical_device.properties();
  auto& frame_context = graphics_module.frame_context();

  ImGui::SeparatorText("Device");

  if (ImGui::BeginTable("##device_table", 2, table_flags)) {
    _setup_label_value_columns();

    _table_row({"Vendor", std::string{sbx::graphics::vendor_name(properties.vendorID)}});
    _table_row({"Renderer", fmt::format("{} ({})", properties.deviceName, sbx::graphics::device_type_name(properties.deviceType))});
    _table_row({"Api version", fmt::format("{}.{}.{}", VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion))});

    if (frame_context.is_initialized()) {
      const auto present_mode = frame_context.swapchain().present_mode();
      _table_row({"Present mode", present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "FIFO"});
    } else {
      _table_row({"Present mode", "not initialized yet"});
    }

    ImGui::EndTable();
  }

  ImGui::SeparatorText("Memory (VRAM)");

  const auto& memory_properties = physical_device.memory_properties();

  auto budgets = std::array<VmaBudget, VK_MAX_MEMORY_HEAPS>{};
  vmaGetHeapBudgets(graphics_module.allocator(), budgets.data());

  auto used = VkDeviceSize{0u};
  auto budget = VkDeviceSize{0u};

  for (auto index = std::uint32_t{0u}; index < memory_properties.memoryHeapCount; ++index) {
    if (memory_properties.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
      used += budgets[index].usage;
      budget += budgets[index].budget;
    }
  }

  if (ImGui::BeginTable("##vram_table", 2, table_flags)) {
    _setup_label_value_columns();

    _table_row({"Used", _format_bytes(used)});
    _table_row({"Total", _format_bytes(physical_device.device_local_memory())});
    _table_row({"Budget (OS-reported)", _format_bytes(budget)});

    ImGui::EndTable();
  }

  ImGui::SeparatorText("Draw calls (last frame)");

  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
  const auto& draw_stats = scene_renderer_module.last_draw_stats();

  if (ImGui::BeginTable("##draw_calls_table", 3, table_flags)) {
    ImGui::TableSetupColumn("Category");
    ImGui::TableSetupColumn("Draw calls");
    ImGui::TableSetupColumn("Instances");
    ImGui::TableHeadersRow();

    _table_row({"Opaque", fmt::format("{}", draw_stats.opaque.draw_calls), fmt::format("{}", draw_stats.opaque.instance_count)});
    _table_row({"Transparent", fmt::format("{}", draw_stats.transparent.draw_calls), fmt::format("{}", draw_stats.transparent.instance_count)});
    _table_row({"Shadow casters", fmt::format("{}", draw_stats.shadow.draw_calls), fmt::format("{}", draw_stats.shadow.instance_count)});

    ImGui::EndTable();
  }

  ImGui::SeparatorText("Resources");

  auto& resource_registry = graphics_module.resource_registry();
  const auto& buffer_pool = resource_registry.pool<sbx::graphics::buffer>();
  const auto& image_pool = resource_registry.pool<sbx::graphics::image>();

  if (ImGui::BeginTable("##resources_table", 4, table_flags)) {
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Live");
    ImGui::TableSetupColumn("Slots");
    ImGui::TableSetupColumn("Pending");
    ImGui::TableHeadersRow();

    _table_row({"Buffers", fmt::format("{}", buffer_pool.size()), fmt::format("{}", buffer_pool.slot_count()), fmt::format("{}", buffer_pool.pending_count())});
    _table_row({"Images", fmt::format("{}", image_pool.size()), fmt::format("{}", image_pool.slot_count()), fmt::format("{}", image_pool.pending_count())});

    ImGui::EndTable();
  }

  ImGui::SeparatorText("Bindless table");

  auto& bindless_table = graphics_module.bindless_table();

  if (ImGui::BeginTable("##bindless_table", 2, table_flags)) {
    _setup_label_value_columns();

    _table_row({"Sampled images", fmt::format("{}", bindless_table.sampled_image_count())});
    _table_row({"Samplers (live)", fmt::format("{}", bindless_table.sampler_count())});
    _table_row({"Samplers (cached)", fmt::format("{}", bindless_table.cached_sampler_count())});
    _table_row({"Storage images", fmt::format("{}", bindless_table.storage_image_count())});
    _table_row({"Sampled cubes", fmt::format("{}", bindless_table.sampled_cube_count())});
    _table_row({"Storage cubes", fmt::format("{}", bindless_table.storage_cube_count())});

    ImGui::EndTable();
  }
}

auto statistics_panel::_draw_performance_tab() -> void {
  const auto delta_time = sbx::core::engine::delta_time();
  const auto frame_time_ms = static_cast<std::float_t>(delta_time) * 1000.0f;

  _smoothed_frame_time_ms = frame_time_ms;
  _smoothed_frame_time_ms.update(delta_time, 10.0f);

  const auto smoothed_ms = _smoothed_frame_time_ms.value();
  const auto fps = smoothed_ms > 0.0f ? 1000.0f / smoothed_ms : 0.0f;

  ImGui::SeparatorText("Frame");

  if (ImGui::BeginTable("##frame_table", 2, table_flags)) {
    _setup_label_value_columns();

    _table_row({"FPS", fmt::format("{:.1f}", fps)});
    _table_row({"Frame time", fmt::format("{:.3f} ms", smoothed_ms)});

    ImGui::EndTable();
  }

  ImGui::SeparatorText("CPU scopes (previous frame)");

  auto entries = std::vector<std::pair<std::string, sbx::utility::scope_stat>>{};

  for (const auto& [name, stat] : sbx::utility::scope_stats()) {
    entries.emplace_back(name, stat);
  }

  if (ImGui::BeginTable("##cpu_scopes_table", 3, table_flags | ImGuiTableFlags_Sortable)) {
    ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_DefaultSort);
    ImGui::TableSetupColumn("Time");
    ImGui::TableSetupColumn("Samples");
    ImGui::TableHeadersRow();

    if (auto* sort_specs = ImGui::TableGetSortSpecs(); sort_specs && sort_specs->SpecsCount > 0) {
      const auto& spec = sort_specs->Specs[0];
      const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

      switch (spec.ColumnIndex) {
        case 0: {
          std::ranges::sort(entries, [ascending](const auto& lhs, const auto& rhs) { return ascending ? lhs.first < rhs.first : lhs.first > rhs.first; });
          break;
        }
        case 1: {
          std::ranges::sort(entries, [ascending](const auto& lhs, const auto& rhs) { return ascending ? lhs.second.last_milliseconds < rhs.second.last_milliseconds : lhs.second.last_milliseconds > rhs.second.last_milliseconds; });
          break;
        }
        case 2: {
          std::ranges::sort(entries, [ascending](const auto& lhs, const auto& rhs) { return ascending ? lhs.second.sample_count < rhs.second.sample_count : lhs.second.sample_count > rhs.second.sample_count; });
          break;
        }
        default: {
          break;
        }
      }
    }

    for (const auto& [name, stat] : entries) {
      _table_row({name, fmt::format("{:.3f} ms", stat.last_milliseconds), fmt::format("{}", stat.sample_count)});
    }

    ImGui::EndTable();
  }

  ImGui::SeparatorText("Resident assets");

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  const auto counts = assets_module.resident_asset_counts();

  if (ImGui::BeginTable("##resident_assets_table", 2, table_flags)) {
    _setup_label_value_columns();

    _table_row({"Textures", fmt::format("{}", counts.textures)});
    _table_row({"Meshes", fmt::format("{}", counts.meshes)});
    _table_row({"Fonts", fmt::format("{}", counts.fonts)});
    _table_row({"Materials", fmt::format("{}", counts.materials)});
    _table_row({"Environment maps", fmt::format("{}", counts.environment_maps)});
    _table_row({"Particle effects", fmt::format("{}", counts.particle_effects)});
    _table_row({"Animation graphs", fmt::format("{}", counts.animation_graphs)});
    _table_row({"Shader graphs", fmt::format("{}", counts.shader_graphs)});
    _table_row({"Skeletons", fmt::format("{}", counts.skeletons)});
    _table_row({"Animation clips", fmt::format("{}", counts.animation_clips)});

    ImGui::EndTable();
  }
}

auto statistics_panel::_draw_memory_tab() -> void {
  if (!memory_stats::is_tracking_enabled()) {
    ImGui::TextDisabled("Rebuild with -DSBX_TRACK_MEMORY=ON to see allocation stats.");
    return;
  }

  const auto total_allocated = memory_stats::total_allocated();
  const auto total_freed = memory_stats::total_freed();
  const auto alloc_count = memory_stats::alloc_count();
  const auto dealloc_count = memory_stats::dealloc_count();

  const auto alloc_delta = static_cast<std::int64_t>(total_allocated - _prev_total_allocated);
  const auto free_delta = static_cast<std::int64_t>(total_freed - _prev_total_freed);
  const auto alloc_count_delta = static_cast<std::int64_t>(alloc_count - _prev_alloc_count);
  const auto dealloc_count_delta = static_cast<std::int64_t>(dealloc_count - _prev_dealloc_count);

  _prev_total_allocated = total_allocated;
  _prev_total_freed = total_freed;
  _prev_alloc_count = alloc_count;
  _prev_dealloc_count = dealloc_count;

  ImGui::SeparatorText("Usage");

  if (ImGui::BeginTable("##memory_usage_table", 2, table_flags)) {
    _setup_label_value_columns();

    _table_row({"Current usage", _format_bytes(memory_stats::current_usage())});
    _table_row({"Peak usage", _format_bytes(memory_stats::peak_usage())});

    ImGui::EndTable();
  }

  ImGui::SeparatorText("Delta (since last frame)");

  if (ImGui::BeginTable("##memory_delta_table", 2, table_flags)) {
    _setup_label_value_columns();

    _table_row({"Allocated", _format_bytes_delta(alloc_delta)});
    _table_row({"Freed", _format_bytes_delta(-free_delta)});
    _table_row({"Net", _format_bytes_delta(alloc_delta - free_delta)});
    _table_row({"Allocations", fmt::format("{:+}", alloc_count_delta)});
    _table_row({"Deallocations", fmt::format("{:+}", dealloc_count_delta)});

    ImGui::EndTable();
  }
}

} // namespace editor
