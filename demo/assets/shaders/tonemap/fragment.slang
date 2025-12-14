#include <libsbx/math/constants.slang>

[[vk::binding(0, 0)]] 
Sampler2D resolve_image;

// [[vk::binding(1, 0)]] 
// Sampler2D bloom_image;

struct push_data {
  float exposure;
  float bloom_mix;
  float saturation;
  float contrast;
  float temperature;
  float tint;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

struct fs_in {
  float2 uv : TEXCOORD0;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

float3 apply_white_balance(float3 c, float temperature, float tint) {
  float t = temperature * 0.1;
  float ti = tint * 0.1;
  float3 w = float3(1.0 + t, 1.0, 1.0 - t);

  w.g *= (1.0 - ti);
  w.b *= (1.0 + ti);

  return c * w;
}

float3 apply_tonemap(float3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;

  return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
fs_out main(fs_in in) {
  fs_out out;

  float3 color = resolve_image.Sample(in.uv).rgb;
  // float3 bloom = bloom_image.Sample(in.uv).rgb;

  // float3 hdr = color + pow(bloom, 1.2) * push.bloom_mix;
  float3 hdr = color;
  hdr = apply_white_balance(hdr, push.temperature, push.tint);

  float3 mapped = apply_tonemap(hdr * exp2(push.exposure));

  float luminance = dot(mapped, float3(0.2126, 0.7152, 0.0722));

  mapped = lerp(float3(luminance, luminance, luminance), mapped, push.saturation);
  
  float3 gray = luminance.xxx;
  mapped = gray + (mapped - gray) * push.contrast;

  out.color = float4(mapped, 1.0);

  return out;
}
