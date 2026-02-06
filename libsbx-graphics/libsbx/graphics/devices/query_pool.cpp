// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/query_pool.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

query_pool::query_pool(const logical_device& logical_device, const VkQueryType type, const std::uint32_t query_count)
: _type{type},
  _query_count{query_count} {
  auto create_info = VkQueryPoolCreateInfo{};
  create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  create_info.queryType = _type;
  create_info.queryCount = _query_count;

  validate(vkCreateQueryPool(logical_device, &create_info, nullptr, &_handle));
}

query_pool::~query_pool() {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& logical_device = graphics_module.logical_device();

  vkDestroyQueryPool(logical_device, _handle, nullptr);
}

auto query_pool::handle() const -> handle_type {
  return _handle;
}

query_pool::operator handle_type() const {
  return _handle;
}

auto query_pool::reset(command_buffer& command_buffer, std::uint32_t first_query, std::uint32_t count) const -> void {
  utility::assert_that(first_query + count <= _query_count, "Invalid reset range");
  vkCmdResetQueryPool(command_buffer, _handle, first_query, (count != 0u) ? count : _query_count);
}

auto query_pool::write_timestamp(command_buffer& command_buffer, const VkPipelineStageFlagBits stage, const std::uint32_t query_index) const -> void {
  vkCmdWriteTimestamp(command_buffer, stage, _handle, query_index);
}

auto query_pool::get_duration(const std::uint32_t begin, const std::uint32_t end) const -> units::millisecond {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& physical_device = graphics_module.physical_device();
  auto& logical_device = graphics_module.logical_device();

  auto timestamps = std::array<std::uint64_t, 2u>{};

  validate(vkGetQueryPoolResults(logical_device, _handle, begin, end - begin + 1u, timestamps.size() * sizeof(std::uint64_t), timestamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

  const auto ns_per_tick = physical_device.properties().limits.timestampPeriod;

  const auto duration = units::nanosecond{(timestamps[1u] - timestamps[0u]) * ns_per_tick};

  return units::quantity_cast<units::millisecond>(duration);
}

} // namespace sbx::graphics

