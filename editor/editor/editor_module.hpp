// SPDX-License-Identifier: MIT
#ifndef EDITOR_EDITOR_MODULE_HPP_
#define EDITOR_EDITOR_MODULE_HPP_

#include <libsbx/core/module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

namespace editor {

class editor_module : public sbx::core::module<editor_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<sbx::graphics::graphics_module, sbx::scenes::scenes_module>{});

public:

  editor_module();

  ~editor_module() override;

  auto update() -> void override;

  auto selected_node() -> sbx::scenes::node {
    return _selected_node;
  }

  auto set_selected_node(sbx::scenes::node node) -> void {
    _selected_node = node;
  }

private:

  sbx::scenes::node _selected_node{sbx::scenes::node::null};

}; // class editor_module

} // namespace editor

#endif // EDITOR_EDITOR_MODULE_HPP_