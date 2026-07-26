// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_DEPTH_PRE_PASS_HPP_
#define LIBSBX_RENDER_DEPTH_PRE_PASS_HPP_

#include <array>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

/** 
 * @brief Depth-only pre-pass: clears + writes the shared depth target from the opaque list. 
 */
class depth_pre_pass final : public render_pass {

public:

  depth_pre_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "depth_pre";
  }

  auto execute(render_context& context) -> void override;

private:

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _pipelines{};

}; // class depth_pre_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_DEPTH_PRE_PASS_HPP_
