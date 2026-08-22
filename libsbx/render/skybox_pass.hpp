// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SKYBOX_PASS_HPP_
#define LIBSBX_RENDER_SKYBOX_PASS_HPP_

#include <array>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

/**
 * @brief Skybox pass: renders the skybox environment.
 *
 * Runs between opaque_pass and transparent_pass, not after (it used to run last): the skybox is
 * a full-screen triangle whose depth is pushed to exactly the far plane and tested less-or-equal
 * against the pre-pass depth, so it only lands on pixels no opaque geometry claimed. Transparent
 * draws never write depth, so a pixel behind a transparent fragment still reads as "empty"
 * (far-plane depth) to that test. Running the skybox after transparent meant it passed that test
 * on exactly those pixels and unconditionally overwrote the (non-blending) skybox pipeline over
 * whatever transparent had already blended there — transparent objects were invisible wherever
 * only sky was behind them. Drawing it before transparent instead makes it part of what
 * transparent blends against.
 */
class skybox_pass final : public render_pass {

public:

  skybox_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Skybox";
  }

  auto execute(render_context& context) -> void override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

}; // class skybox_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_SKYBOX_PASS_HPP_
