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

float3 apply_tonemap(float3 value) {
  static const float a = 2.51;
  static const float b = 0.03;
  static const float c = 2.43;
  static const float d = 0.59;
  static const float e = 0.14;

  return saturate((value * (a * value + b)) / (value * (c * value + d) + e));
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float3 color = resolve_image.Sample(input.uv).rgb;
  // float3 bloom = bloom_image.Sample(input.uv).rgb;

  color = apply_white_balance(color, push.temperature, push.tint);

  float3 exposed = color * exp2(push.exposure);

  // exposed += pow(bloom, 1.2) * push.bloom_mix;

  float3 mapped = apply_tonemap(exposed);

  float luminance = dot(mapped, float3(0.2126, 0.7152, 0.0722));

  mapped = lerp(luminance.xxx, mapped, push.saturation);
  mapped = luminance.xxx + (mapped - luminance.xxx) * push.contrast;

  output.color = float4(mapped, 1.0);

  return output;
}
