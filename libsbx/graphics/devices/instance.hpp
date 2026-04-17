// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_INSTANCE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_INSTANCE_HPP_

#include <vulkan/vulkan.hpp>

#include <libsbx/utility/noncopyable.hpp>

namespace sbx::graphics {

class instance : public utility::noncopyable {

public:

  instance();

  ~instance();

  auto handle() const noexcept -> const VkInstance&;

  operator const VkInstance&() const noexcept;

private:

  VkInstance _handle{};

}; // class instance

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_INSTANCE_HPP_
