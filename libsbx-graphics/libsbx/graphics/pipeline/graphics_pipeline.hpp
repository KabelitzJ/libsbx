#ifndef LIBSBX_GRAPHICS_PIPELINE_GRAPHICS_PIPELINE_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_GRAPHICS_PIPELINE_HPP_

#include <optional>
#include <filesystem>
#include <cinttypes>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>

#include <vulkan/vulkan.hpp>

#include <fmt/format.h>

#include <libsbx/utility/enum.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/graphics/buffers/buffer.hpp>
#include <libsbx/graphics/buffers/uniform_handler.hpp>

#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/pipeline/compiler.hpp>
#include <libsbx/graphics/pipeline/vertex_input_description.hpp>

#include <libsbx/graphics/descriptor/descriptor.hpp>
#include <libsbx/graphics/descriptor/descriptor_set.hpp>

#include <libsbx/graphics/images/image2d.hpp>

#include <libsbx/graphics/render_graph.hpp>
#include <libsbx/graphics/resource_storage.hpp>

namespace sbx::graphics {

enum class polygon_mode : std::int32_t {
  fill = VK_POLYGON_MODE_FILL,
  line = VK_POLYGON_MODE_LINE,
  point = VK_POLYGON_MODE_POINT
}; // enum class polygon_mode

enum class cull_mode : std::int32_t {
  none = VK_CULL_MODE_NONE,
  front = VK_CULL_MODE_FRONT_BIT,
  back = VK_CULL_MODE_BACK_BIT,
  front_and_back = VK_CULL_MODE_FRONT_AND_BACK
}; // enum class cull_mode

enum class front_face : std::int32_t {
  counter_clockwise = VK_FRONT_FACE_COUNTER_CLOCKWISE,
  clockwise = VK_FRONT_FACE_CLOCKWISE
}; // enum class front_face

struct depth_bias {
  std::float_t constant_factor{0.0f};
  std::float_t clamp{0.0f};
  std::float_t slope_factor{0.0f};
}; // struct depth_bias

struct rasterization_state {
  graphics::polygon_mode polygon_mode{graphics::polygon_mode::fill};
  std::float_t line_width{1.0f};
  graphics::cull_mode cull_mode{graphics::cull_mode::back};
  graphics::front_face front_face{graphics::front_face::counter_clockwise};
  std::optional<graphics::depth_bias> depth_bias{};
}; // struct rasterization_state

enum class primitive_topology : std::int32_t {
  point_list = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
  line_list = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
  line_strip = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
  triangle_list = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  triangle_strip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
  triangle_fan = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
  line_list_with_adjacency = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
  line_strip_with_adjacency = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
  triangle_list_with_adjacency = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
  triangle_strip_with_adjacency = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
  patch_list = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
}; // enum class primitive_topology

enum class depth : std::int32_t {
  disabled = 0,
  read_write = 1,
  read_only = 2
}; // enum class depth_test

enum class compare_operation : std::int32_t {
  never = VK_COMPARE_OP_NEVER,
  less = VK_COMPARE_OP_LESS,
  equal = VK_COMPARE_OP_EQUAL,
  less_or_equal = VK_COMPARE_OP_LESS_OR_EQUAL,
  greater = VK_COMPARE_OP_GREATER,
  not_equal = VK_COMPARE_OP_NOT_EQUAL,
  greater_or_equal = VK_COMPARE_OP_GREATER_OR_EQUAL,
  always = VK_COMPARE_OP_ALWAYS
}; // enum class compare_operation

struct pipeline_definition {
  graphics::depth depth{graphics::depth::read_write};
  graphics::compare_operation compare_operation{graphics::compare_operation::less_or_equal};
  bool uses_transparency{false};
  graphics::rasterization_state rasterization_state{};
  graphics::primitive_topology primitive_topology{graphics::primitive_topology::triangle_list};
  vertex_input_description vertex_input{};
}; // struct pipeline_definition

class graphics_pipeline : public pipeline {

  using base = pipeline;

  struct rendering_create_info {
    VkPipelineRenderingCreateInfo info;
    std::vector<VkFormat> color_formats;
    VkFormat depth_format;
    VkFormat stencil_format;
  }; // struct rendering_create_info

public:

  struct compiled_shaders {
    std::string name{};
    std::unordered_map<SlangStage, std::vector<std::uint32_t>> shader_codes{};
  }; // struct compiled_shaders

  graphics_pipeline(const std::filesystem::path& path, const std::vector<attachment_description>& attachments, const pipeline_definition& default_definition = pipeline_definition{}, const VkSpecializationInfo* specialization_info = nullptr);

  graphics_pipeline(const compiled_shaders& shaders, const std::vector<attachment_description>& attachments, const pipeline_definition& default_definition = pipeline_definition{}, const VkSpecializationInfo* specialization_info = nullptr);

  ~graphics_pipeline() override;

  auto handle() const noexcept -> VkPipeline override;

  auto has_variable_descriptors() const noexcept -> bool override {
    return _has_variable_descriptors;
  }

  auto descriptor_counts(std::uint32_t set) const noexcept -> std::vector<std::uint32_t> override {
    auto counts = std::vector<std::uint32_t>{};

    for (const auto& binding_data : _set_data[set].binding_data) {
      counts.push_back(binding_data.descriptor_count);
    }

    return counts;
  }

  auto descriptor_set_layout(std::uint32_t set) const noexcept -> VkDescriptorSetLayout override;

  auto descriptor_pool() const noexcept -> VkDescriptorPool override;

  auto layout() const noexcept -> VkPipelineLayout override;

  auto bind_point() const noexcept -> VkPipelineBindPoint override;

  auto descriptor_block(const std::string& name, std::uint32_t set) const -> const shader::uniform_block& override {
    if (auto it = _set_data[set].uniform_blocks.find(name); it != _set_data[set].uniform_blocks.end()) {
      return it->second;
    }

    throw std::runtime_error(fmt::format("Failed to find descriptor block '{}' in graphics pipeline '{}'", name, _name));
  }

  auto push_constant() const noexcept -> const std::optional<shader::uniform_block>& override {
    return _push_constant;
  }

  auto find_descriptor_binding(const std::string& name, std::uint32_t set) const -> std::optional<std::uint32_t> override {
    if (auto it = _set_data[set].descriptor_bindings.find(name); it != _set_data[set].descriptor_bindings.end()) {
      return it->second;
    }

    return std::nullopt;
  }

  auto find_descriptor_type_at_binding(std::uint32_t set, std::uint32_t binding) const -> std::optional<VkDescriptorType> override {
    if (_set_data[set].binding_data.size() <= binding) {
      return std::nullopt;
    }

    return _set_data[set].binding_data[binding].descriptor_type;
  }

  auto rendering_info() -> const rendering_create_info& {
    return _rendering_info;
  }

private:

  struct per_binding_data {
    VkDescriptorType descriptor_type;
    std::uint32_t descriptor_count;
  }; // struct per_binding_data

  struct per_set_data {
    std::unordered_map<std::string, shader::uniform> uniforms;
    std::unordered_map<std::string, shader::uniform_block> uniform_blocks;
    std::unordered_map<std::string, std::uint32_t> descriptor_bindings;
    std::unordered_map<std::string, std::uint32_t> descriptor_sizes;
    std::vector<per_binding_data> binding_data;
    VkDescriptorSetLayout layout;
  }; // struct per_set_data

  auto _initialize(const std::vector<attachment_description>& attachments, const pipeline_definition& definition, const VkSpecializationInfo* specialization_info) -> void;

  auto _update_definition(const std::filesystem::path& path, const pipeline_definition default_definition) -> pipeline_definition;

  auto _get_stage_from_name(const std::string& name) const noexcept -> VkShaderStageFlagBits;

  std::unordered_map<VkShaderStageFlagBits, std::unique_ptr<shader>> _shaders{};

  std::vector<per_set_data> _set_data;
  std::optional<shader::uniform_block> _push_constant;

  std::string _name{};
  VkPipelineLayout _layout{};
  VkPipeline _handle{};
  VkPipelineBindPoint _bind_point{};
  bool _has_variable_descriptors{};

  rendering_create_info _rendering_info;

  VkDescriptorPool _descriptor_pool{};

}; // class graphics_pipeline

using graphics_pipeline_handle = resource_handle<graphics_pipeline>;

} // namespace sbx::graphics

template<>
struct sbx::utility::enum_mapping<sbx::graphics::polygon_mode> {

  using entry_type = sbx::utility::entry<sbx::graphics::polygon_mode>;

  static constexpr auto values = std::array<entry_type, 3u>{
    entry_type{sbx::graphics::polygon_mode::fill, "fill"},
    entry_type{sbx::graphics::polygon_mode::line, "line"},
    entry_type{sbx::graphics::polygon_mode::point, "point"}
  };

}; // struct sbx::utility::enum_mapping

template<>
struct sbx::utility::enum_mapping<sbx::graphics::depth> {

  using entry_type = sbx::utility::entry<sbx::graphics::depth>;

  static constexpr auto values = std::array<entry_type, 3u>{
    entry_type{sbx::graphics::depth::disabled, "disabled"},
    entry_type{sbx::graphics::depth::read_write, "read_write"},
    entry_type{sbx::graphics::depth::read_only, "read_only"}
  };

}; // struct sbx::utility::enum_mapping

template<>
struct sbx::utility::enum_mapping<sbx::graphics::cull_mode> {

  using entry_type = sbx::utility::entry<sbx::graphics::cull_mode>;

  static constexpr auto values = std::array<entry_type, 4u>{
    entry_type{sbx::graphics::cull_mode::back, "back"},
    entry_type{sbx::graphics::cull_mode::front, "front"},
    entry_type{sbx::graphics::cull_mode::front_and_back, "front_and_back"},
    entry_type{sbx::graphics::cull_mode::none, "none"}
  };

}; // struct sbx::utility::enum_mapping

template<>
struct sbx::utility::enum_mapping<sbx::graphics::front_face> {

  using entry_type = sbx::utility::entry<sbx::graphics::front_face>;

  static constexpr auto values = std::array<entry_type, 2u>{
    entry_type{sbx::graphics::front_face::clockwise, "clockwise"},
    entry_type{sbx::graphics::front_face::counter_clockwise, "counter_clockwise"}
  };

}; // struct sbx::utility::enum_mapping

#endif // LIBSBX_GRAPHICS_PIPELINE_GRAPHICS_PIPELINE_HPP_
