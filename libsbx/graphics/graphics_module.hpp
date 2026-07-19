// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_
#define LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>
#include <libsbx/graphics/devices/allocator.hpp>

namespace sbx::graphics {

class graphics_module : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<platform::platform_module>;

  graphics_module();

  ~graphics_module();

  [[nodiscard]] auto instance() noexcept -> graphics::instance& {
    return _instance;
  }

  [[nodiscard]] auto physical_device() noexcept -> graphics::physical_device& {
    return _physical_device;
  }

  [[nodiscard]] auto logical_device() noexcept -> graphics::logical_device& {
    return _logical_device;
  }

  [[nodiscard]] auto allocator() noexcept -> graphics::allocator& {
    return _allocator;
  }

private:

  // Construction order is destruction order in reverse — members depend on
  // the ones declared above them.
  graphics::instance _instance;
  graphics::physical_device _physical_device;
  graphics::logical_device _logical_device;
  graphics::allocator _allocator;

}; // class graphics_module

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_
