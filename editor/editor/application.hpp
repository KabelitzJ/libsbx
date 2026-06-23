// SPDX-License-Identifier: MIT
#ifndef EDITOR_APPLICATION_HPP_
#define EDITOR_APPLICATION_HPP_

#include <libsbx/units/units.hpp>
#include <libsbx/utility/utility.hpp>
#include <libsbx/math/math.hpp>
#include <libsbx/core/core.hpp>
#include <libsbx/devices/devices.hpp>
#include <libsbx/graphics/graphics.hpp>
#include <libsbx/models/models.hpp>
#include <libsbx/ui/ui.hpp>
#include <libsbx/scenes/scenes.hpp>

#include <editor/editor_context.hpp>
#include <editor/panels/hierarchy_panel.hpp>
#include <editor/panels/inspector_panel.hpp>
#include <editor/panels/viewport_panel.hpp>

namespace editor {

class application : public sbx::core::application {

public:

  application();

  ~application() override;

  auto update() -> void override;

  auto fixed_update() -> void override;

  auto is_paused() const -> bool override;

private:

  bool _is_paused;

  sbx::math::uuid _cube_mesh;
  sbx::math::uuid _cube_material;

}; // class application

} // namespace editor

#endif // EDITOR_APPLICATION_HPP_
