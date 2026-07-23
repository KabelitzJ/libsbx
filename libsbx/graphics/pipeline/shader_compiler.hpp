// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_SHADER_COMPILER_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_SHADER_COMPILER_HPP_

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#include <libsbx/utility/noncopyable.hpp>

namespace sbx::graphics {

/**
 * @brief Compiles Slang source to SPIR-V at runtime via the Slang API.
 *
 * One global session for the process. A module may hold several entry points (one file, a vertex
 * and a fragment function); the caller names the ones it wants and their stages, and gets a
 * SPIR-V blob per entry point.
 */
class shader_compiler : public utility::noncopyable {

public:

  struct entry_point_request {
    std::string name;
    VkShaderStageFlagBits stage;

    auto operator==(const entry_point_request&) const -> bool = default;

  }; // struct entry_point_request

  struct compiled_entry_point {
    VkShaderStageFlagBits stage;
    std::string name;
    std::vector<std::uint32_t> spirv;
  }; // struct compiled_entry_point

  shader_compiler();

  ~shader_compiler();

  [[nodiscard]] auto compile(const std::filesystem::path& path, std::span<const entry_point_request> entry_points) -> std::vector<compiled_entry_point>;

private:

  Slang::ComPtr<slang::IGlobalSession> _global_session{};

}; // class shader_compiler

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_SHADER_COMPILER_HPP_
