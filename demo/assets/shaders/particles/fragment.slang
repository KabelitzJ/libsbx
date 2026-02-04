#include <libsbx/common/scene_data.slang>
#include <libsbx/common/image.slang>

#include <particles/common.slang>

struct fs_in {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] float4 color : COLOR;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
  float4 brightness : SV_Target1;
}; // struct fs_out

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(3, 0)]]
Sampler2D<float4> image;

[[vk::push_constant]]
ConstantBuffer<render_params> params;

static const float bloom_threshold = 1.0;

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float4 sampled_color = float4(0, 0, 0, 1);

  sampled_color = image.Sample(input.uv);

  float4 color = sampled_color * input.color;

  // clip(color.a - 0.9);

  output.color = color;

  float brightness = dot(output.color.rgb, float3(0.2126, 0.7152, 0.0722));

  output.brightness = (brightness > bloom_threshold) ? float4(output.color.rgb, 1.0) : float4(0.0, 0.0, 0.0, 1.0);

  return output;
}
