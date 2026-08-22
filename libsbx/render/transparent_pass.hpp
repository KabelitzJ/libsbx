// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_TRANSPARENT_PASS_HPP_
#define LIBSBX_RENDER_TRANSPARENT_PASS_HPP_

#include <array>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

/**
 * @brief Transparent forward color pass: LOADs color (written by opaque_pass/skybox_pass) and
 * depth (test only, no write), draws context.packet->transparent_commands back-to-front.
 *
 * Pipeline slots: [0] cull_mode::back — single-sided objects, and reused for the front-faces
 * half of double-sided objects (identical GPU state either way). [1] cull_mode::front — the
 * back-faces half of double-sided objects. A double-sided object is submitted as two
 * draw_commands, pipeline 1 (back faces) before pipeline 0 (front faces), so its own back faces
 * composite before its own front faces — see render_module.cpp's _build_packet for why that
 * needs two entries sharing a sort key and a stable sort.
 */
class transparent_pass final : public render_pass {

public:

  transparent_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Transparent";
  }

  auto execute(render_context& context) -> void override;

private:

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _pipelines{};

}; // class transparent_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_TRANSPARENT_PASS_HPP_
