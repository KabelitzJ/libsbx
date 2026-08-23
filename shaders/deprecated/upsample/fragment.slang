#include <libsbx/math/constants.slang>

[[vk::binding(0, 0)]]
Sampler2D low_image;

[[vk::binding(1, 0)]]
Sampler2D base_image;

struct push_data {
  float intensity;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

struct fs_in {
  float2 uv : TEXCOORD0;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float3 low = low_image.Sample(input.uv).rgb;
  float3 base = base_image.Sample(input.uv).rgb;

  float3 result = base + low * push.intensity;

  output.color = float4(result, 1.0);

  return output;
}
