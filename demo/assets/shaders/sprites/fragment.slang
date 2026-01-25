#include <libsbx/common/scene_data.slang>
#include <libsbx/common/image.slang>

struct fs_in {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] nointerpolation uint instance_id : INSTANCE_ID;
};

struct fs_out {
  float4 color      : SV_Target0; // HDR scene color
  float4 brightness : SV_Target1; // bloom source
};

struct sprite_data {
  float4x4 model;
  float4 base_color;
  float2 size;
  uint albedo_image_index;
  uint emissive_image_index;
  float4 emissive_factor;
  float emissive_strength;
  uint padding0;
  uint padding1;
  uint padding2;
}; // struct sprite_data

struct push_data {
  Ptr<sprite_data> sprites;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]]
SamplerState sampler;

[[vk::binding(2, 0)]]
Texture2D<float4> images[sbx::common::max_image_array_size];

static const float bloom_threshold = 1.0;

float4 get_albedo(fs_in input, sprite_data data) {
  uint image_index = data.albedo_image_index;

  if (image_index >= sbx::common::max_image_array_size) {
    return data.base_color;
  }

  float4 color = images[NonUniformResourceIndex(image_index)].Sample(sampler, input.uv);

  return color * data.base_color;
}

float3 get_emissive(fs_in input, sprite_data data) {
  uint image_index = data.emissive_image_index;

  float3 emissive = data.emissive_factor.rgb * data.emissive_strength;

  if (image_index >= sbx::common::max_image_array_size) {
    return emissive;
  }

  return emissive * images[NonUniformResourceIndex(image_index)].Sample(sampler, input.uv).rgb;
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  sprite_data sprite = push.sprites[input.instance_id];

  float4 albedo = get_albedo(input, sprite);
  float3 emissive = get_emissive(input, sprite);

  output.color = float4(albedo.rgb + emissive, albedo.a);
  
  float brightness = dot(output.color.rgb, float3(0.2126, 0.7152, 0.0722));

  output.brightness = (brightness > 1.0) ? float4(output.color.rgb, 1.0) : float4(0.0, 0.0, 0.0, 1.0);

  return output;
}
