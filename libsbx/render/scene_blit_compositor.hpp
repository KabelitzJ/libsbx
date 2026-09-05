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
 * @brief Default compositor: blits scene_renderer_module::final_image() to the swapchain.
 *
 * Clears the swapchain instead if nothing was rendered this frame (@ref
 * scene_renderer_module::has_rendered). Registered automatically by scene_renderer_module's
 * constructor, so apps need no code of their own to present the scene.
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
