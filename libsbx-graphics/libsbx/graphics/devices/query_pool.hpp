#ifndef LIBSBX_GRAPHICS_DEVICES_QUERY_POOL_HPP_
#define LIBSBX_GRAPHICS_DEVICES_QUERY_POOL_HPP_

#include <vulkan/vulkan.h>

#include <libsbx/units/time.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>

namespace sbx::graphics {

class query_pool {

public:

  using handle_type = VkQueryPool;

  query_pool(const logical_device& logical_device, const VkQueryType type, const std::uint32_t query_count);

  query_pool(const query_pool&) = delete;

  ~query_pool();

  auto operator=(const query_pool&) -> query_pool& = delete;

  auto handle() const -> handle_type;

  operator handle_type() const;

  auto reset(command_buffer& command_buffer) const -> void;

  auto write_timestamp(command_buffer& command_buffer, const VkPipelineStageFlagBits stage, const std::uint32_t query_index) const -> void;

  auto get_duration(const std::uint32_t begin, const std::uint32_t end) const -> units::millisecond;

private:

  handle_type _handle;
  VkQueryType _type;
  std::uint32_t _query_count;

}; // class query_pool

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_QUERY_POOL_HPP_
