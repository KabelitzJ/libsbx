// SPDX-License-Identifier: MIT
#ifndef LIBSBX_EDITOR_PROFILER_PANEL_HPP_
#define LIBSBX_EDITOR_PROFILER_PANEL_HPP_

#include <vector>

#include <libsbx/units/time.hpp>

#include <libsbx/utility/iterator.hpp>

#include <libsbx/core/profiler.hpp>

namespace sbx::editor {

class profiler_panel {

  using child_map = std::vector<std::vector<core::scope_info::node_id>>;

  template<typename Type>
  using sampler_vector = std::vector<core::sampler<Type>>;

public:

  profiler_panel();

  auto render() -> void;

private:

  auto _delta(const sampler_vector<std::uint64_t>& time_samplers, const sampler_vector<std::double_t>& percent_samplers, const core::scope_info& info_a, const core::scope_info& info_b, const std::uint32_t column_user_id) -> std::int32_t;

  auto _populate_nodes(std::span<const core::scope_info> infos, child_map& children_map, std::vector<core::scope_info::node_id>& root_nodes) -> void;

  auto _render_node(const sampler_vector<std::uint64_t>& time_samplers, const sampler_vector<std::double_t>& percent_samplers, const core::scope_info::node_id node_id, const std::span<const core::scope_info>& all_nodes, const child_map& child_map) -> void;

  sbx::units::second _time;
  std::uint16_t _frames;
  std::uint16_t _fps;

}; // class profiler_panel

} // namespace sbx::editor

#endif // LIBSBX_EDITOR_PROFILER_PANEL_HPP_
