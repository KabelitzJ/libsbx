// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_TYPES_HPP_
#define LIBSBX_GRAPHICS_TYPES_HPP_

#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

#include <vulkan/vulkan.h>

#include <libsbx/reflection/annotations.hpp>
#include <libsbx/reflection/enum.hpp>

namespace sbx::graphics {

enum class [[=reflection::named]] format : std::int32_t {
  undefined = VK_FORMAT_UNDEFINED,
  r8_unorm = VK_FORMAT_R8_UNORM,
  r16_sfloat = VK_FORMAT_R16_SFLOAT,
  r16_unorm = VK_FORMAT_R16_UNORM,
  r32_sfloat = VK_FORMAT_R32_SFLOAT,
  r32_uint = VK_FORMAT_R32_UINT,
  r64_uint = VK_FORMAT_R64_UINT,
  r16g16_sfloat = VK_FORMAT_R16G16_SFLOAT,
  r32g32_sfloat = VK_FORMAT_R32G32_SFLOAT,
  r32g32_uint = VK_FORMAT_R32G32_UINT,
  r8g8b8a8_unorm = VK_FORMAT_R8G8B8A8_UNORM,
  r8g8b8a8_srgb = VK_FORMAT_R8G8B8A8_SRGB,
  b8g8r8a8_unorm = VK_FORMAT_B8G8R8A8_UNORM,
  b8g8r8a8_srgb = VK_FORMAT_B8G8R8A8_SRGB,
  a2b10g10r10_unorm_pack32 = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
  r16g16b16a16_sfloat = VK_FORMAT_R16G16B16A16_SFLOAT,
  r32g32b32a32_sfloat = VK_FORMAT_R32G32B32A32_SFLOAT,
  d16_unorm = VK_FORMAT_D16_UNORM,
  d32_sfloat = VK_FORMAT_D32_SFLOAT,
  d24_unorm_s8_uint = VK_FORMAT_D24_UNORM_S8_UINT,
  d32_sfloat_s8_uint = VK_FORMAT_D32_SFLOAT_S8_UINT
}; // enum class format

/**
 * @brief Where a resource's memory lives and how the host intends to touch it.
 */
enum class memory_usage : std::uint8_t {
  /** @brief Device local, not host visible. The default for anything the GPU reads repeatedly. */
  device_local,
  /** @brief Host visible and persistently mapped for sequential writes. Staging and per frame uploads. */
  host_write,
  /** @brief Host visible and persistently mapped for random access. Readback. */
  host_read
}; // enum class memory_usage

enum class [[=reflection::bit_field]] buffer_usage : std::uint32_t {
  transfer_source = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
  transfer_destination = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
  uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
  storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
  index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
  vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
  indirect = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
  device_address = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
}; // enum class buffer_usage

enum class [[=reflection::bit_field]] image_usage : std::uint32_t {
  transfer_source = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
  transfer_destination = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
  sampled = VK_IMAGE_USAGE_SAMPLED_BIT,
  storage = VK_IMAGE_USAGE_STORAGE_BIT,
  color_attachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
  depth_stencil_attachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
  transient_attachment = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
  input_attachment = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
}; // enum class image_usage

enum class image_view_type : std::int32_t {
  one_dimensional = VK_IMAGE_VIEW_TYPE_1D,
  two_dimensional = VK_IMAGE_VIEW_TYPE_2D,
  three_dimensional = VK_IMAGE_VIEW_TYPE_3D,
  cube = VK_IMAGE_VIEW_TYPE_CUBE,
  one_dimensional_array = VK_IMAGE_VIEW_TYPE_1D_ARRAY,
  two_dimensional_array = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
  cube_array = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
}; // enum class image_view_type

enum class image_layout : std::int32_t {
  undefined = VK_IMAGE_LAYOUT_UNDEFINED,
  general = VK_IMAGE_LAYOUT_GENERAL,
  color_attachment_optimal = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  depth_stencil_attachment_optimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  depth_stencil_read_only_optimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
  depth_attachment_optimal = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
  depth_read_only_optimal = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
  shader_read_only_optimal = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  transfer_source_optimal = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
  transfer_destination_optimal = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
  present_source = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  read_only_optimal = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
  attachment_optimal = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
  rendering_local_read = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ
}; // enum class image_layout

enum class blend_factor : std::int32_t {
  zero = VK_BLEND_FACTOR_ZERO,
  one = VK_BLEND_FACTOR_ONE,
  source_color = VK_BLEND_FACTOR_SRC_COLOR,
  one_minus_source_color = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
  destination_color = VK_BLEND_FACTOR_DST_COLOR,
  one_minus_destination_color = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
  source_alpha = VK_BLEND_FACTOR_SRC_ALPHA,
  one_minus_source_alpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
  destination_alpha = VK_BLEND_FACTOR_DST_ALPHA,
  one_minus_destination_alpha = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
  constant_color = VK_BLEND_FACTOR_CONSTANT_COLOR,
  one_minus_constant_color = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
  constant_alpha = VK_BLEND_FACTOR_CONSTANT_ALPHA,
  one_minus_constant_alpha = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
  source_alpha_saturate = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE
}; // enum class blend_factor

enum class [[=reflection::bit_field]] color_component : std::uint32_t {
  r = VK_COLOR_COMPONENT_R_BIT,
  g = VK_COLOR_COMPONENT_G_BIT,
  b = VK_COLOR_COMPONENT_B_BIT,
  a = VK_COLOR_COMPONENT_A_BIT
}; // enum class color_component

enum class blend_operation : std::int32_t {
  add = VK_BLEND_OP_ADD,
  subtract = VK_BLEND_OP_SUBTRACT,
  reverse_subtract = VK_BLEND_OP_REVERSE_SUBTRACT,
  min = VK_BLEND_OP_MIN,
  max = VK_BLEND_OP_MAX
}; // enum class blend_operation

enum class attachment_load_op : std::int32_t {
  load = VK_ATTACHMENT_LOAD_OP_LOAD,
  clear = VK_ATTACHMENT_LOAD_OP_CLEAR,
  dont_care = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
  none = VK_ATTACHMENT_LOAD_OP_NONE
}; // enum class attachment_load_op

enum class attachment_store_op : std::int32_t {
  store = VK_ATTACHMENT_STORE_OP_STORE,
  dont_care = VK_ATTACHMENT_STORE_OP_DONT_CARE,
  none = VK_ATTACHMENT_STORE_OP_NONE
}; // enum class attachment_store_op

enum class pipeline_bind_point : std::int32_t {
  graphics = VK_PIPELINE_BIND_POINT_GRAPHICS,
  compute = VK_PIPELINE_BIND_POINT_COMPUTE,
  ray_tracing = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
}; // enum class pipeline_bind_point

enum class [[=reflection::named]] address_mode : std::int32_t {
  repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
  mirror = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
  clamp_to_edge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  clamp_to_border = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
}; // enum class address_mode

enum class [[=reflection::named]] filter : std::int32_t {
  nearest = VK_FILTER_NEAREST,
  linear = VK_FILTER_LINEAR
}; // enum class filter

enum class mipmap_mode : std::int32_t {
  nearest = VK_SAMPLER_MIPMAP_MODE_NEAREST,
  linear = VK_SAMPLER_MIPMAP_MODE_LINEAR
}; // enum class mipmap_mode

struct lod_clamp {
  inline static constexpr auto none = std::float_t{VK_LOD_CLAMP_NONE};
}; // struct lod_clamp

enum class samples : std::int32_t {
  count_1 = VK_SAMPLE_COUNT_1_BIT,
  count_2 = VK_SAMPLE_COUNT_2_BIT,
  count_4 = VK_SAMPLE_COUNT_4_BIT,
  count_8 = VK_SAMPLE_COUNT_8_BIT,
  count_16 = VK_SAMPLE_COUNT_16_BIT,
  count_32 = VK_SAMPLE_COUNT_32_BIT,
  count_64 = VK_SAMPLE_COUNT_64_BIT
}; // enum class samples

enum class [[=reflection::named]] polygon_mode : std::int32_t {
  fill = VK_POLYGON_MODE_FILL,
  line = VK_POLYGON_MODE_LINE,
  point = VK_POLYGON_MODE_POINT
}; // enum class polygon_mode

enum class [[=reflection::named]] cull_mode : std::int32_t {
  none = VK_CULL_MODE_NONE,
  front = VK_CULL_MODE_FRONT_BIT,
  back = VK_CULL_MODE_BACK_BIT,
  front_and_back = VK_CULL_MODE_FRONT_AND_BACK
}; // enum class cull_mode

enum class [[=reflection::named]] front_face : std::int32_t {
  counter_clockwise = VK_FRONT_FACE_COUNTER_CLOCKWISE,
  clockwise = VK_FRONT_FACE_CLOCKWISE
}; // enum class front_face

struct depth_bias {
  std::float_t constant_factor{0.0f};
  std::float_t slope_factor{0.0f};
  std::float_t clamp{0.0f};
}; // struct depth_bias

struct rasterization_state {
  graphics::polygon_mode polygon_mode{graphics::polygon_mode::fill};
  std::float_t line_width{1.0f};
  graphics::cull_mode cull_mode{graphics::cull_mode::back};
  graphics::front_face front_face{graphics::front_face::counter_clockwise};
  std::optional<graphics::depth_bias> depth_bias{};
}; // struct rasterization_state

enum class [[=reflection::named]] primitive_topology : std::int32_t {
  point_list = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
  line_list = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
  line_strip = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
  triangle_list = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  triangle_strip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
  triangle_fan = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
  patch_list = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
}; // enum class primitive_topology


enum class [[=reflection::bit_field]] pipeline_stage : std::uint64_t {
  none = VK_PIPELINE_STAGE_2_NONE,
  draw_indirect = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
  vertex_shader = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
  early_fragment_tests = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
  fragment_shader = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
  late_fragment_tests = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
  color_attachment_output = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
  compute_shader = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
  transfer = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
  bottom_of_pipe = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
  all_commands = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
}; // enum class pipeline_stage

/**
 * @brief Synchronization2 access flags (`VkAccessFlags2`/`VkAccessFlagBits2`) — same 64-bit
 * rationale as @ref pipeline_stage.
 */
enum class [[=reflection::bit_field]] access : std::uint64_t {
  none = VK_ACCESS_2_NONE,
  indirect_command_read = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
  shader_read = VK_ACCESS_2_SHADER_READ_BIT,
  shader_write = VK_ACCESS_2_SHADER_WRITE_BIT,
  color_attachment_read = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
  color_attachment_write = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
  depth_stencil_attachment_read = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
  depth_stencil_attachment_write = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
  transfer_read = VK_ACCESS_2_TRANSFER_READ_BIT,
  transfer_write = VK_ACCESS_2_TRANSFER_WRITE_BIT,
  host_write = VK_ACCESS_2_HOST_WRITE_BIT,
  memory_read = VK_ACCESS_2_MEMORY_READ_BIT,
  memory_write = VK_ACCESS_2_MEMORY_WRITE_BIT,
  shader_sampled_read = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
  shader_storage_write = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
}; // enum class access

/** @brief A depth/stencil attachment's clear value — the render-side equivalent of
 * `VkClearDepthStencilValue`, kept Vulkan-free like the rest of this header. Color clears use
 * math::color instead, there being no need for a dedicated wrapper. */
struct depth_stencil_clear_value {
  std::float_t depth{1.0f};
  std::uint32_t stencil{0u};
}; // struct depth_stencil_clear_value

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

/**
 * @brief Converts one of this header's wrapper enums to its Vulkan counterpart, pairing each
 * target kind with the underlying type that actually matches it: a plain Vulkan C enum (VkFormat,
 * VkImageLayout, ...) always has underlying type `int`, so its wrapper is `std::int32_t`-backed;
 * `VkFlags` (used for 32-bit bitmasks like VkBufferUsageFlags) is `std::uint32_t`; `VkFlags64`
 * (used for synchronization2's wider bitmasks like VkPipelineStageFlags2) is `std::uint64_t`.
 */
template<typename VkEnum, typename Enum>
requires (
  std::is_enum_v<Enum> &&
  (
    (std::is_enum_v<VkEnum> && std::is_same_v<std::underlying_type_t<Enum>, std::int32_t>) ||
    (std::is_same_v<VkEnum, VkFlags> && std::is_same_v<std::underlying_type_t<Enum>, std::uint32_t>) ||
    (std::is_same_v<VkEnum, VkFlags64> && std::is_same_v<std::underlying_type_t<Enum>, std::uint64_t>)
  )
)
constexpr auto to_vk_enum(Enum value) -> VkEnum {
  return static_cast<VkEnum>(value);
}

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_TYPES_HPP_
