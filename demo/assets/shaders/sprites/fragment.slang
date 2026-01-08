#include <libsbx/common/scene_data.slang>
#include <libsbx/common/image.slang>

struct fs_in {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] float4 color : COLOR;
  [[vk::location(2)]] nointerpolation uint image_index : IMAGE_INDEX;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
  float4 brightness : SV_Target1;
}; // struct fs_out

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]]
SamplerState sampler;

[[vk::binding(2, 0)]]
Texture2D<float4> images[sbx::common::max_image_array_size];

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float4 color = input.color;

  if (input.image_index < sbx::common::max_image_array_size) {
    color *= images[NonUniformResourceIndex(input.image_index)].Sample(sampler, input.uv);
  }

  output.color = color;
  output.brightness = float4(0,0,0,1);

  return output;
}