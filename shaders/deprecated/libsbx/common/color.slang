#ifndef COMMON_COLOR_SLANG_
#define COMMON_COLOR_SLANG_

namespace sbx::common {

struct color {
  static const float4 red = float4(1, 0, 0, 1);
  static const float4 green = float4(0, 1, 0, 1);
  static const float4 blue = float4(0, 0, 1, 1);
}; // struct color

} // namespace sbx::common

#endif // COMMON_COLOR_SLANG_
