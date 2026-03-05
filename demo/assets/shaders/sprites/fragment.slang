#include <libsbx/common/scene_data.slang>
#include <libsbx/common/image.slang>

#include <sprites/common.slang>

struct fs_in {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] nointerpolation uint instance_id : INSTANCE_ID;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
  float4 brightness : SV_Target1;
}; // struct fs_out

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

float median(float r, float g, float b) {
  return max(min(r, g), min(max(r, g), b));
}

float4 get_albedo(fs_in input, sprite_data data) {
  uint image_index = data.albedo_image_index;

  if (image_index >= sbx::common::max_image_array_size) {
    return data.base_color;
  }

  if ((data.flags & flag_msdf_text) != 0) {
    float3 msdf = images[NonUniformResourceIndex(image_index)].Sample(sampler, input.uv).rgb;
    float sd = median(msdf.r, msdf.g, msdf.b);

    float2 uv_size = data.uv_max - data.uv_min;
    uint2 tex_size;
    images[NonUniformResourceIndex(image_index)].GetDimensions(tex_size.x, tex_size.y);
    float2 glyph_size_px = uv_size * float2(tex_size);
    float screen_px_range = max(data.size.x / glyph_size_px.x, data.size.y / glyph_size_px.y) * 2.0;
    screen_px_range = max(screen_px_range, 1.0);

    float screen_px_distance = screen_px_range * (sd - 0.5);
    float opacity = clamp(screen_px_distance + 0.5, 0.0, 1.0);

    return float4(data.base_color.rgb, data.base_color.a * opacity);
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

  if ((sprite.flags & flag_msdf_text) != 0) {
    clip(albedo.a - 0.01);
  } else {
    clip(albedo.a - 0.9);
  }

  float3 emissive = get_emissive(input, sprite);

  output.color = float4(albedo.rgb + emissive, albedo.a);

  float brightness = dot(output.color.rgb, float3(0.2126, 0.7152, 0.0722));

  output.brightness = (brightness > bloom_threshold) ? float4(output.color.rgb, 1.0) : float4(0.0, 0.0, 0.0, 1.0);

  return output;
}
