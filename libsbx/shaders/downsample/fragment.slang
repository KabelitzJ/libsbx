#include <libsbx/math/constants.slang>

[[vk::binding(0, 0)]]
Sampler2D source_image;

struct fs_in {
  float2 uv : TEXCOORD0;
};

struct fs_out {
  float4 color : SV_Target0;
};

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  uint w, h;
  source_image.GetDimensions(w, h);

  float2 texel = float2(1.0 / float(w), 1.0 / float(h));

  float3 color0 = source_image.Sample(input.uv + texel * float2(-0.5, -0.5)).rgb;
  float3 color1 = source_image.Sample(input.uv + texel * float2( 0.5, -0.5)).rgb;
  float3 color2 = source_image.Sample(input.uv + texel * float2(-0.5,  0.5)).rgb;
  float3 color3 = source_image.Sample(input.uv + texel * float2( 0.5,  0.5)).rgb;

  float3 avg = (color0 + color1 + color2 + color3) * 0.25;

  output.color = float4(avg, 1.0);

  return output;
}
