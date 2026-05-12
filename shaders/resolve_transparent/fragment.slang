#include <libsbx/math/constants.slang>

struct fs_in {
  float2 uv : TEXCOORD0;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
  float4 brightness : SV_Target1;
}; // struct fs_out

[[vk::binding(0, 0)]] Sampler2D<float4> accumulator_image;
[[vk::binding(1, 0)]] Sampler2D<float> revealage_image;

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float4 accumulator = accumulator_image.Sample(input.uv);
  float revealage = revealage_image.Sample(input.uv);

  float3 average = accumulator.rgb / max(accumulator.a, sbx::math::epsilon);

  output.color = float4(average, 1.0f - revealage);
  output.brightness = float4(0.0, 0.0, 0.0, 1.0);

  return output;
}
