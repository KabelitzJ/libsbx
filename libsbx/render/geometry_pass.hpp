// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_GEOMETRY_PASS_HPP_
#define LIBSBX_RENDER_GEOMETRY_PASS_HPP_

#include <array>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

/** 
 * @brief Forward color pass: loads the pre-pass depth (test, no write), draws opaque then transparent. 
 */
class geometry_pass final : public render_pass {

public:

  geometry_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "geometry";
  }

  auto execute(render_context& context) -> void override;

private:

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _opaque_pipelines{};
  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _transparent_pipelines{};

}; // class geometry_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_GEOMETRY_PASS_HPP_
