#include <libsbx/math/constants.slang>

[[vk::binding(0, 0)]]
Sampler2D resolve_image;

[[vk::binding(1, 0)]]
Sampler2D bloom_image;

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

static const float3x3 aces_input = float3x3(
  0.59719, 0.35458, 0.04823,
  0.07600, 0.90834, 0.01566,
  0.02840, 0.13383, 0.83777
);

static const float3x3 aces_output = float3x3(
   1.60475, -0.53108, -0.07367,
  -0.10208,  1.10813, -0.00605,
  -0.00327, -0.07276,  1.07602
);

float3 rrt_and_odt_fit(float3 color) {
  float3 a = color * (color + 0.0245786) - 0.000090537;
  float3 b = color * (0.983729 * color + 0.4329510) + 0.238081;

  return a / b;
}

static const float aces_pre_scale = 1.25;

float3 apply_aces_tonemap(float3 value) {
  value *= aces_pre_scale;
  value /= 0.6;

  value = mul(aces_input, value);
  value = rrt_and_odt_fit(value);
  value = mul(aces_output, value);

  return clamp(value, 0.0, 1.0);
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float3 color = resolve_image.Sample(input.uv).rgb;
  float3 bloom = bloom_image.Sample(input.uv).rgb;

  color = apply_white_balance(color, push.temperature, push.tint);

  float3 exposed = (color + bloom * push.bloom_mix) * exp2(push.exposure);

  float3 mapped = apply_aces_tonemap(exposed);

  float luminance = dot(mapped, float3(0.2126, 0.7152, 0.0722));

  mapped = lerp(luminance.xxx, mapped, push.saturation);
  mapped = (mapped - 0.5) * push.contrast + 0.5;
  mapped = saturate(mapped);

  output.color = float4(mapped, 1.0);

  return output;
}
