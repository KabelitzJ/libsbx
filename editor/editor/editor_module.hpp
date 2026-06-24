// SPDX-License-Identifier: MIT
#ifndef EDITOR_EDITOR_MODULE_HPP_
#define EDITOR_EDITOR_MODULE_HPP_

#include <variant>

#include <libsbx/math/uuid.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene_graph.hpp>

namespace editor {

struct asset_selection {
  sbx::math::uuid id{sbx::math::uuid::nil()};
}; // struct asset_selection

struct node_selection {
  sbx::scenes::node node{sbx::scenes::node::null};
}; // struct node_selection

using selection = std::variant<std::monostate, node_selection, asset_selection>;

class editor_module : public sbx::core::module<editor_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<sbx::graphics::graphics_module, sbx::scenes::scenes_module>{});

public:

  editor_module();

  ~editor_module() override;

  auto update() -> void override;

  auto selection() const -> const editor::selection& {
    return _selection;
  }

  auto set_selection(const editor::selection& selection) -> void {
    _selection = selection;
  }

  auto clear_selection() -> void {
    _selection = std::monostate{};
  }

private:

  editor::selection _selection{std::monostate{}};

}; // class editor_module

} // namespace editor

#endif // EDITOR_EDITOR_MODULE_HPP_