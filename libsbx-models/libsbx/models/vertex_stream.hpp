// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_VERTEX_STREAM_HPP_
#define LIBSBX_MODELS_VERTEX_STREAM_HPP_

#include <array>
#include <cstdint>
#include <string_view>

#include <libsbx/utility/enum.hpp>

#include <libsbx/reflection/description.hpp>

namespace sbx::models {

enum class vertex_stream : std::uint8_t {
  none = 0,
  color = utility::bit_v<0>,
  custom0 = utility::bit_v<1>,
  custom1 = utility::bit_v<2>,
  custom2 = utility::bit_v<3>,
  custom3 = utility::bit_v<4>
}; // enum class vertex_stream

inline constexpr auto operator|(const vertex_stream lhs, const vertex_stream rhs) -> vertex_stream {
  return static_cast<vertex_stream>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

inline constexpr auto vertex_stream_count = std::size_t{5u};

inline constexpr auto vertex_stream_element_size = std::uint32_t{16u};

struct vertex_stream_descriptor {
  vertex_stream value;
  std::string_view name;
  std::string_view push_name;
  std::string_view define;
}; // struct vertex_stream_descriptor

inline constexpr auto vertex_stream_descriptors = std::array<vertex_stream_descriptor, vertex_stream_count>{
  vertex_stream_descriptor{vertex_stream::color, "color",   "color_buffer", "VERTEX_STREAM_COLOR"},
  vertex_stream_descriptor{vertex_stream::custom0, "custom0", "custom0_buffer", "VERTEX_STREAM_CUSTOM0"},
  vertex_stream_descriptor{vertex_stream::custom1, "custom1", "custom1_buffer", "VERTEX_STREAM_CUSTOM1"},
  vertex_stream_descriptor{vertex_stream::custom2, "custom2", "custom2_buffer", "VERTEX_STREAM_CUSTOM2"},
  vertex_stream_descriptor{vertex_stream::custom3, "custom3", "custom3_buffer", "VERTEX_STREAM_CUSTOM3"}
};

inline constexpr auto vertex_stream_index(const vertex_stream stream) -> std::uint32_t {
  for (auto i = std::uint32_t{0u}; i < static_cast<std::uint32_t>(vertex_stream_descriptors.size()); ++i) {
    if (vertex_stream_descriptors[i].value == stream) {
      return i;
    }
  }

  return ~std::uint32_t{0u};
}

} // namespace sbx::models

template<>
struct sbx::utility::is_bit_field<sbx::models::vertex_stream> : std::true_type { };

template<>
struct sbx::reflection::description<sbx::models::vertex_stream> {

  static constexpr auto name() -> std::string_view {
    return "vertex_stream";
  }

  static constexpr auto enumerators() {
    return std::make_tuple(
      enumerator{"none", sbx::models::vertex_stream::none},
      enumerator{"color", sbx::models::vertex_stream::color},
      enumerator{"custom0", sbx::models::vertex_stream::custom0},
      enumerator{"custom1", sbx::models::vertex_stream::custom1},
      enumerator{"custom2", sbx::models::vertex_stream::custom2},
      enumerator{"custom3", sbx::models::vertex_stream::custom3}
    );
  }

}; // struct sbx::reflection::description<sbx::graphics::vertex_stream>

#endif // LIBSBX_MODELS_VERTEX_STREAM_HPP_
