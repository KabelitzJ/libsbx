#ifndef EDITOR_EDITOR_MODULE_HPP_
#define EDITOR_EDITOR_MODULE_HPP_

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/render_module.hpp>

namespace editor {

class editor_module final : public sbx::utility::noncopyable {

public:

  using dependencies = sbx::core::dependency_list<sbx::graphics::graphics_module, sbx::assets::assets_module, sbx::scenes::scenes_module, sbx::render::render_module>;

  editor_module() {

  }

  ~editor_module() {

  }

  auto update() -> void {
    
  }

private:

}; // class editor_module

} // namespace editor

#endif // EDITOR_EDITOR_MODULE_HPP_
