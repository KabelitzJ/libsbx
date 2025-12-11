#ifndef LIBSBX_GRAPHICS_SUBRENDERER_HPP_
#define LIBSBX_GRAPHICS_SUBRENDERER_HPP_

#include <cmath>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/render_graph.hpp>

namespace sbx::graphics {

class subrenderer {

public:

  subrenderer() { }

  virtual ~subrenderer() = default;

  virtual auto render(command_buffer& command_buffer) -> void = 0;

}; // class subrenderer

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_SUBRENDERER_HPP_
