// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_DEVICES_QUERY_POOL_HPP_
#define LIBSBX_GRAPHICS_DEVICES_QUERY_POOL_HPP_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/devices/logical_device.hpp>

namespace sbx::graphics {

/**
 * @brief A VkQueryPool of a single type (timestamp or pipeline statistics).
 *
 * @p pipeline_statistics is only consulted for VK_QUERY_TYPE_PIPELINE_STATISTICS, where it also
 * determines values_per_query() (one std::uint64_t per set bit, per Vulkan's own layout).
 */
class query_pool : public utility::noncopyable {

public:

  using handle_type = VkQueryPool;

  query_pool(const logical_device& logical_device, VkQueryType type, std::uint32_t query_count, VkQueryPipelineStatisticFlags pipeline_statistics = 0u);

  ~query_pool();

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

  [[nodiscard]] auto query_count() const noexcept -> std::uint32_t {
    return _query_count;
  }

  [[nodiscard]] auto values_per_query() const noexcept -> std::uint32_t {
    return _values_per_query;
  }

  /**
   * @brief Reads @p query_count 64-bit results starting at @p first_query without blocking.
   *
   * Returns false (leaving @p out untouched) if any queried slot's result isn't available yet,
   * instead of stalling the caller -- callers only read a slot once its owning frame-in-flight
   * has already cycled back around, so this should always succeed in practice, but a query that
   * was reset and never re-written this frame (e.g. a disabled pass) still reports unavailable.
   */
  [[nodiscard]] auto try_read_results(std::uint32_t first_query, std::uint32_t query_count, std::vector<std::uint64_t>& out) const -> bool;

private:

  VkDevice _device{};
  handle_type _handle{};
  std::uint32_t _query_count{};
  std::uint32_t _values_per_query{1u};

}; // class query_pool

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_QUERY_POOL_HPP_
