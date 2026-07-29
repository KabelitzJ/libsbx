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
