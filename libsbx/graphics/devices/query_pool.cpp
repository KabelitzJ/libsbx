// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/devices/query_pool.hpp>

#include <bit>

#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

query_pool::query_pool(const logical_device& logical_device, VkQueryType type, std::uint32_t query_count, VkQueryPipelineStatisticFlags pipeline_statistics)
: _device{logical_device},
  _query_count{query_count},
  _values_per_query{type == VK_QUERY_TYPE_PIPELINE_STATISTICS ? static_cast<std::uint32_t>(std::popcount(static_cast<std::uint32_t>(pipeline_statistics))) : 1u} {
  auto create_info = VkQueryPoolCreateInfo{};
  create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  create_info.queryType = type;
  create_info.queryCount = query_count;
  create_info.pipelineStatistics = pipeline_statistics;

  validate(vkCreateQueryPool(_device, &create_info, nullptr, &_handle), "vkCreateQueryPool");
}

query_pool::~query_pool() {
  vkDestroyQueryPool(_device, _handle, nullptr);
}

auto query_pool::try_read_results(std::uint32_t first_query, std::uint32_t query_count, std::vector<std::uint64_t>& out) const -> bool {
  const auto value_count = query_count * _values_per_query;
  auto results = std::vector<std::uint64_t>(value_count);

  const auto result = vkGetQueryPoolResults(
    _device, _handle, first_query, query_count,
    results.size() * sizeof(std::uint64_t), results.data(), sizeof(std::uint64_t),
    VK_QUERY_RESULT_64_BIT
  );

  if (result == VK_NOT_READY) {
    return false;
  }

  validate(result, "vkGetQueryPoolResults");

  out = std::move(results);

  return true;
}

} // namespace sbx::graphics
