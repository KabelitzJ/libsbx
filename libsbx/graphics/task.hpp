// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_TASK_HPP_
#define LIBSBX_GRAPHICS_TASK_HPP_

#include <libsbx/graphics/commands/command_buffer.hpp>

namespace sbx::graphics {

class task {

public:

  task() = default;

  virtual ~task() = default;

  virtual auto execute(command_buffer& command_buffer) -> void = 0;

}; // class task

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_TASK_HPP_
