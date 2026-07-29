// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PRESENT_PASS_HPP_
#define LIBSBX_RENDER_PRESENT_PASS_HPP_

#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

/**
 * @brief Default composite: samples the finished scene image and writes it to the swapchain. The
 * application may replace this pass (render_module::set_composite_pass) to composite the scene
 * image differently — e.g. an editor drawing it into an ImGui viewport.
 */
class present_pass final : public render_pass {

public:

  present_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "present";
  }

  auto execute(render_context& context) -> void override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

}; // class present_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PRESENT_PASS_HPP_
