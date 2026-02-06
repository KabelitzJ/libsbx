// SPDX-License-Identifier: MIT
#include <libsbx/editor/profiler_panel.hpp>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::editor {

auto profiler_panel::_populate_nodes(std::span<const core::scope_info> infos, child_map& children_map, std::vector<core::scope_info::node_id>& root_nodes) -> void {
  children_map.resize(core::scope_info::max_nodes);

  for (auto& entry : children_map) {
    entry.clear();
  }

  root_nodes.clear();

  for (const auto& info : infos) {
    if (info.time == core::scope_info::null_time) {
      continue;
    }

    if (info.parent_id == core::scope_info::null_node) {
      root_nodes.push_back(info.id);
    } else {
      children_map[info.parent_id].push_back(info.id);
    }
  }
}

static auto _percentage(const std::int64_t part, const std::int64_t total) -> std::double_t {
  return total == 0 ? 0.0 : (static_cast<std::double_t>(part) * 100.0) / static_cast<std::double_t>(total);
}

static auto _node_percentage(std::span<const core::scope_info> infos, const core::scope_info& info) -> std::double_t {
  return info.parent_id == core::scope_info::null_node ? 0.0 : _percentage(info.time.count(), infos[info.parent_id].time.count());
}

struct columns {
  inline static constexpr auto scope = std::uint32_t{0};
  inline static constexpr auto time = std::uint32_t{1};
  inline static constexpr auto percent = std::uint32_t{2};
  inline static constexpr auto source = std::uint32_t{3};
}; // struct columns

auto profiler_panel::_delta(const sampler_vector<std::uint64_t>& time_samplers, const sampler_vector<std::double_t>& percent_samplers, const core::scope_info& info_a, const core::scope_info& info_b, const std::uint32_t column_user_id) -> std::int32_t {
  if (column_user_id == columns::scope) {
    return info_a.label.compare(info_b.label);
  }

  if (column_user_id == columns::time) {
    const auto time_a = time_samplers[info_a.id].average_as<std::double_t>();
    const auto time_b = time_samplers[info_b.id].average_as<std::double_t>();

    return (time_a > time_b) ? 1 : (time_a < time_b) ? -1 : 0;
  }

  if (column_user_id == columns::percent) {
    const auto percent_a = percent_samplers[info_a.id].average_as<std::double_t>();
    const auto percent_b = percent_samplers[info_b.id].average_as<std::double_t>();

    return (percent_a > percent_b) ? 1 : (percent_a < percent_b) ? -1 : 0;
  }

  return 0;
}

static auto _lerp_color(const ImVec4& a, const ImVec4& b, std::float_t t) {
  return ImVec4{
    a.x + (b.x - a.x) * t,
    a.y + (b.y - a.y) * t,
    a.z + (b.z - a.z) * t,
    a.w + (b.w - a.w) * t
  };
}

static auto _get_color_for_time(const units::millisecond ms) -> ImVec4 {
  static constexpr auto green = ImVec4{0.40f, 0.85f, 0.40f, 1.0f};
  static constexpr auto yellow = ImVec4{0.95f, 0.85f, 0.20f, 1.0f};
  static constexpr auto red = ImVec4{0.90f, 0.25f, 0.25f, 1.0f};

  if (ms <= 10.0f) {
    return green;
  } else if (ms <= 16.66f) {
    const auto t = (ms - 10.0f) / (16.66f - 10.0f);
    return _lerp_color(green, yellow, t);
  } else if (ms <= 33.33f) {
    const auto t = (ms - 16.66f) / (33.33f - 16.66f);
    return _lerp_color(yellow, red, t);
  } else {
    return red;
  }
}

auto profiler_panel::_render_node(const sampler_vector<std::uint64_t>& time_samplers, const sampler_vector<std::double_t>& percent_samplers, const core::scope_info::node_id node_id, const std::span<const core::scope_info>& all_nodes, const child_map& child_map) -> void {
  const auto& info = all_nodes[node_id];
  const auto& children = child_map[node_id];

  ImGui::TableNextRow();

  ImGui::TableSetColumnIndex(columns::scope);

  auto node_flags = ImGuiTreeNodeFlags{ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen};

  if (children.empty()) {
    node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  constexpr const char spaces[33] = "                                ";
  const char* spaces_ptr  = spaces + sizeof(spaces) - 1u - (info.depth * 2);

  const bool is_node_open = ImGui::TreeNodeEx(reinterpret_cast<void*>(node_id), node_flags, "%s", info.label.data());

  ImGui::TableSetColumnIndex(columns::time);
  const auto time = (time_samplers[node_id].average_as<std::double_t>() / 1000.0);
  ImGui::PushStyleColor(ImGuiCol_Text, _get_color_for_time(units::millisecond{time}));
  ImGui::Text("%s%.3f", spaces_ptr, time);
  ImGui::PopStyleColor();

  ImGui::TableSetColumnIndex(columns::percent);

  if (info.parent_id != core::scope_info::null_node) {
    if (const auto& parent_info = all_nodes[info.parent_id]; parent_info.time.count() > 0) {
      ImGui::Text("%s%.1f%%", spaces_ptr + 1, percent_samplers[node_id].average_as<std::double_t>());
    } else {
      ImGui::Text("%sN/A", spaces_ptr + 1);
    }
  } else {
    ImGui::Text(" ");
  }

  if (is_node_open && !children.empty()) {
    for (const auto child_id : children) {
      _render_node(time_samplers, percent_samplers, child_id, all_nodes, child_map);
    }

    ImGui::TreePop(); 
  }
}

profiler_panel::profiler_panel() {

}

auto profiler_panel::render() -> void {
  auto& graphics_module = sbx::core::engine::get_module<graphics::graphics_module>();

  ImGui::Begin("Profiler");

  const auto delta_time = core::engine::delta_time();

  _time += delta_time;
  ++_frames;

  if (_time >= sbx::units::second{1}) {
    _fps = _frames;
    _time = sbx::units::second{0};
    _frames = 0;
  }

  const auto dt_cpu = units::quantity_cast<sbx::units::millisecond>(delta_time);
  // const auto dt_gpu = graphics_module.gpu_frame_time();

  if (ImGui::BeginTable("timings_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 150.0f);
    ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::Text("FPS");

    ImGui::TableSetColumnIndex(1); 
    ImGui::Text("%d", _fps);

    ImGui::TableNextRow();
    
    ImGui::TableSetColumnIndex(0); 
    ImGui::Text("Delta time [CPU]");
    
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, _get_color_for_time(dt_cpu));
    ImGui::Text("%.3f [ms]", static_cast<std::double_t>(dt_cpu.value()));
    ImGui::PopStyleColor();

    for (const auto& [scope, timing] : graphics_module.gpu_timings()) {
      ImGui::TableNextRow();
    
      ImGui::TableSetColumnIndex(0); 
      ImGui::Text("%s", scope.data());

      ImGui::TableSetColumnIndex(1);
      ImGui::PushStyleColor(ImGuiCol_Text, _get_color_for_time(timing));
      ImGui::Text("%.3f [ms]", static_cast<std::double_t>(timing.value()));
      ImGui::PopStyleColor();
    }

    ImGui::EndTable();
  }

  const auto scope_infos = core::scope_infos();

  if (scope_infos.empty()) {
    ImGui::Text("No profiling data captured for this thread.");
    ImGui::End();
    return;
  }

  static thread_local auto children_map = utility::make_vector<std::vector<core::scope_info::node_id>>(core::scope_info::max_nodes);
  static thread_local auto root_nodes = std::vector<core::scope_info::node_id>{};
  static thread_local auto node_time_samplers = utility::make_vector<core::sampler<std::uint64_t>>(core::scope_info::max_nodes, core::sampler<std::uint64_t>{64u});
  static thread_local auto node_percent_samplers = utility::make_vector<core::sampler<std::double_t>>(core::scope_info::max_nodes, core::sampler<std::double_t>{64u});

  _populate_nodes(scope_infos, children_map, root_nodes);

  for (auto i = 0u; i < scope_infos.size(); ++i) {
    if (scope_infos[i].time.count() < 0) {
      continue;
    }

    node_time_samplers[i].record(static_cast<std::uint64_t>(scope_infos[i].time.count()));
    node_percent_samplers[i].record(_node_percentage(scope_infos, scope_infos[i]));
  }

  if (ImGui::BeginTable("ProfilerTreeView", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
    ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 120.0f, columns::scope);
    ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthStretch, 120.0f, columns::time);
    ImGui::TableSetupColumn("% of Parent", ImGuiTableColumnFlags_WidthStretch, 80.0f, columns::percent);

    ImGui::TableHeadersRow();

    if (auto* specs = ImGui::TableGetSortSpecs()) {
      const auto node_comparer = [&](const core::scope_info::node_id a, const core::scope_info::node_id b) -> bool {
        const auto& info_a = scope_infos[a];
        const auto& info_b = scope_infos[b];

        for (int i = 0; i < specs->SpecsCount; ++i) {
          const auto* sort_spec = &specs->Specs[i];
          const int delta = _delta(node_time_samplers, node_percent_samplers, info_a, info_b, sort_spec->ColumnUserID);

          if (delta == 0) {
            continue;
          }

          return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? (delta < 0) : (delta > 0);
        }

        return false;
      };

      for (auto& entry : children_map) {
        std::sort(entry.begin(), entry.end(), node_comparer);
      }

      std::sort(root_nodes.begin(), root_nodes.end(), node_comparer);
    }

    for (const auto id : root_nodes) {
      _render_node(node_time_samplers, node_percent_samplers, id, scope_infos, children_map);
    }

    ImGui::EndTable();
  }

  ImGui::End();
}

} // namespace sbx::editor