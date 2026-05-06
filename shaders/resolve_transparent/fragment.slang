#include <libsbx/math/constants.slang>

struct fs_in {
  float2 uv : TEXCOORD0;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
  float4 brightness : SV_Target1;
}; // struct fs_out

[[vk::binding(0, 0)]] Sampler2D<float4> accum_image;
[[vk::binding(1, 0)]] Sampler2D<float> revealage_image;

bool is_approximately_equal(float a, float b) {
  return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * sbx::math::epsilon;
}

float max3(float3 vector) {
  return max(max(vector.x, vector.y), vector.z);
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float4 accumulation = accum_image.Sample(input.uv);
  float revealage = revealage_image.Sample(input.uv);

  float3 average = accumulation.rgb / max(accumulation.a, sbx::math::epsilon);

  output.color = float4(average, 1.0f - revealage);
  output.brightness = float4(0.0, 0.0, 0.0, 1.0);

  return output;
}
