// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SCENE_BLIT_COMPOSITOR_HPP_
#define LIBSBX_RENDER_SCENE_BLIT_COMPOSITOR_HPP_

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/compositor.hpp>
#include <libsbx/render/scene_renderer_module.hpp>

namespace sbx::render {

/**
 * @brief Default compositor: samples scene_renderer_module::final_image() and writes it to the
 * swapchain — or, if nothing was rendered this frame (see scene_renderer_module::has_rendered),
 * just clears it, same as the old present_pass did for "no active camera". Successor to the old
 * render_pass-based present_pass, built against the leaner compositor_context instead.
 * scene_renderer_module registers one of these with presentation_module::set_compositor
 * automatically in its own constructor, so an app that wants the scene actually presented
 * (demo/runtime) needs no code of its own for that.
 */
class scene_blit_compositor final : public compositor {

public:

  explicit scene_blit_compositor(scene_renderer_module& owner);

  auto execute(compositor_context& context) -> void override;

private:

  scene_renderer_module& _owner;
  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

}; // class scene_blit_compositor

} // namespace sbx::render

#endif // LIBSBX_RENDER_SCENE_BLIT_COMPOSITOR_HPP_
