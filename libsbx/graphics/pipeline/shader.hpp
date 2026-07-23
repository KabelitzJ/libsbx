// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PIPELINE_SHADER_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_SHADER_HPP_

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::graphics {

/**
 * @brief The compiled stages of one Slang file, as VkShaderModules ready to feed a pipeline.
 */
class shader : public utility::noncopyable {

public:

  using id_type = std::uint32_t;

  struct stage {
    VkShaderStageFlagBits stage;
    VkShaderModule module;
    std::string entry_point;
  }; // struct stage

  shader(const std::filesystem::path& path, std::span<const shader_compiler::entry_point_request> entry_points, id_type id);

  ~shader();

  [[nodiscard]] auto id() const noexcept -> id_type {
    return _id;
  }

  [[nodiscard]] auto stages() const noexcept -> const std::vector<stage>& {
    return _stages;
  }

  /**
   * @brief Builds the pipeline stage create-infos. The returned pName pointers reference this
   * shader's storage, so it must outlive pipeline creation (it does — the pipeline is built from
   * it synchronously).
   */
  [[nodiscard]] auto stage_create_infos() const -> std::vector<VkPipelineShaderStageCreateInfo>;

private:

  id_type _id{0u};
  std::vector<stage> _stages{};

}; // class shader

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_PIPELINE_SHADER_HPP_
