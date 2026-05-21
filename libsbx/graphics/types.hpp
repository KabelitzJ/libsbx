#ifndef LIBSBX_GRAPHICS_TYPES_HPP_
#define LIBSBX_GRAPHICS_TYPES_HPP_

#include <cinttypes>

#include <vulkan/vulkan.h>

#include <libsbx/utility/enum.hpp>

#include <libsbx/reflection/description.hpp>

namespace sbx::graphics {

enum class format : std::int32_t {
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
  r16g16b16_sfloat = VK_FORMAT_R16G16B16_SFLOAT,
  r8g8b8a8_unorm = VK_FORMAT_R8G8B8A8_UNORM,
  r8g8b8a8_srgb = VK_FORMAT_R8G8B8A8_SRGB,
  b8g8r8a8_srgb = VK_FORMAT_B8G8R8A8_SRGB,
  a2b10g10r10_unorm_pack32 = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
  r16g16b16a16_sfloat = VK_FORMAT_R16G16B16A16_SFLOAT,
  r32g32b32a32_sfloat = VK_FORMAT_R32G32B32A32_SFLOAT
}; // enum class format

enum class address_mode : std::int32_t {
  repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
  mirror = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
  clamp_to_edge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  clamp_to_border = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
}; // enum class address_mode

enum class filter : std::int32_t {
  nearest = VK_SAMPLER_MIPMAP_MODE_NEAREST,
  linear = VK_SAMPLER_MIPMAP_MODE_LINEAR
}; // enum class filter

enum class usage : std::int32_t {
  transfer_source = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
  transfer_destination = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
  sampled = VK_IMAGE_USAGE_SAMPLED_BIT,
  storage = VK_IMAGE_USAGE_STORAGE_BIT,
  color_attachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
  depth_stencil_attachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
  transient_attachment = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
  input_attachment = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
  fragment_shading_rate_attachment = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
  fragment_density_map = VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT,
  video_decode_destination = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR,
  video_decode_source = VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
  video_decode_dpb = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
  video_encode_destination = VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR,
  video_encode_source = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR,
  video_encode_dpb = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR,
  invocation_mask = VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI,
  attachment_feedback_loop = VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT,
  sample_weight = VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM,
  sample_block_match = VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM,
  host_transfer = VK_IMAGE_USAGE_HOST_TRANSFER_BIT,
  tensor_aliasing = VK_IMAGE_USAGE_TENSOR_ALIASING_BIT_ARM,
  video_encode_quantization_delta_map = VK_IMAGE_USAGE_VIDEO_ENCODE_QUANTIZATION_DELTA_MAP_BIT_KHR,
  video_encode_emphasis_map = VK_IMAGE_USAGE_VIDEO_ENCODE_EMPHASIS_MAP_BIT_KHR,
  tile_memory = VK_IMAGE_USAGE_TILE_MEMORY_BIT_QCOM,
  shading_rate_image = VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV,
  host_transfer_ext = VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT
}; // enum class usage

[[nodiscard]] constexpr auto operator|(usage lhs, usage rhs) -> usage {
  return sbx::utility::from_underlying<usage>(sbx::utility::to_underlying(lhs) | sbx::utility::to_underlying(rhs));
}

[[nodiscard]] constexpr auto operator|=(usage& lhs, usage rhs) -> usage& {
  lhs = lhs | rhs;

  return lhs;
}

[[nodiscard]] constexpr auto operator&(usage lhs, usage rhs) -> usage {
  return sbx::utility::from_underlying<usage>(sbx::utility::to_underlying(lhs) & sbx::utility::to_underlying(rhs));
}

[[nodiscard]] constexpr auto operator&=(usage& lhs, usage rhs) -> usage& {
  lhs = lhs & rhs;

  return lhs;
}

enum class samples : std::int32_t {
  count_1 = VK_SAMPLE_COUNT_1_BIT,
  count_2 = VK_SAMPLE_COUNT_2_BIT,
  count_4 = VK_SAMPLE_COUNT_4_BIT,
  count_8 = VK_SAMPLE_COUNT_8_BIT,
  count_16 = VK_SAMPLE_COUNT_16_BIT,
  count_32 = VK_SAMPLE_COUNT_32_BIT,
  count_64 = VK_SAMPLE_COUNT_64_BIT
}; // enum class samples

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

} // namespace sbx::graphics

template<>
struct sbx::utility::is_bit_field<sbx::graphics::usage> : std::true_type { };

template<>
struct sbx::reflection::description<sbx::graphics::format> {

  static constexpr auto name() -> std::string_view {
    return "format";
  }

  static constexpr auto enumerators() {
    return std::make_tuple(
      enumerator{"undefined", sbx::graphics::format::undefined},
      enumerator{"r8_unorm", sbx::graphics::format::r8_unorm},
      enumerator{"r16_sfloat", sbx::graphics::format::r16_sfloat},
      enumerator{"r16_unorm", sbx::graphics::format::r16_unorm},
      enumerator{"r32_sfloat", sbx::graphics::format::r32_sfloat},
      enumerator{"r32_uint", sbx::graphics::format::r32_uint},
      enumerator{"r64_uint", sbx::graphics::format::r64_uint},
      enumerator{"r16g16_sfloat", sbx::graphics::format::r16g16_sfloat},
      enumerator{"r32g32_sfloat", sbx::graphics::format::r32g32_sfloat},
      enumerator{"r32g32_uint", sbx::graphics::format::r32g32_uint},
      enumerator{"r8g8b8a8_unorm", sbx::graphics::format::r8g8b8a8_unorm},
      enumerator{"r8g8b8a8_srgb", sbx::graphics::format::r8g8b8a8_srgb},
      enumerator{"b8g8r8a8_srgb", sbx::graphics::format::b8g8r8a8_srgb},
      enumerator{"a2b10g10r10_unorm_pack32", sbx::graphics::format::a2b10g10r10_unorm_pack32},
      enumerator{"r16g16b16a16_sfloat", sbx::graphics::format::r16g16b16a16_sfloat},
      enumerator{"r32g32b32a32_sfloat", sbx::graphics::format::r32g32b32a32_sfloat}
    );
  }

}; // struct sbx::reflection::description<sbx::graphics::format>

template<>
struct sbx::reflection::description<sbx::graphics::polygon_mode> {

  static constexpr auto name() -> std::string_view {
    return "polygon_mode";
  }

  static constexpr auto enumerators() {
    return std::make_tuple(
      enumerator{"fill", sbx::graphics::polygon_mode::fill},
      enumerator{"line", sbx::graphics::polygon_mode::line},
      enumerator{"point", sbx::graphics::polygon_mode::point}
    );
  }

}; // struct sbx::reflection::description<sbx::graphics::polygon_mode>

template<>
struct sbx::reflection::description<sbx::graphics::cull_mode> {

  static constexpr auto name() -> std::string_view {
    return "cull_mode";
  }

  static constexpr auto enumerators() {
    return std::make_tuple(
      enumerator{"back", sbx::graphics::cull_mode::back},
      enumerator{"front", sbx::graphics::cull_mode::front},
      enumerator{"front_and_back", sbx::graphics::cull_mode::front_and_back},
      enumerator{"none", sbx::graphics::cull_mode::none}
    );
  }

}; // struct sbx::reflection::description<sbx::graphics::cull_mode>

template<>
struct sbx::reflection::description<sbx::graphics::front_face> {

  static constexpr auto name() -> std::string_view {
    return "front_face";
  }

  static constexpr auto enumerators() {
    return std::make_tuple(
      enumerator{"clockwise", sbx::graphics::front_face::clockwise},
      enumerator{"counter_clockwise", sbx::graphics::front_face::counter_clockwise}
    );
  }

}; // struct sbx::reflection::description<sbx::graphics::front_face>

#endif // LIBSBX_GRAPHICS_TYPES_HPP_