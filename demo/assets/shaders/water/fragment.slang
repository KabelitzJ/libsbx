#include <libsbx/common/scene_data.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 world_xz : WORLD_XZ;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 accum : SV_Target0;
  float revealage : SV_Target1;
}; // struct fs_out

struct push_data {
  float chunk_world_x;
  float chunk_world_z;
  float chunk_size;
  float water_level;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

float hash(float2 p) {
  float3 p3 = fract(float3(p.x, p.y, p.x) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(float2 p) {
  float2 i = floor(p);
  float2 f = fract(p);
  float2 u = f * f * (3.0 - 2.0 * f);

  float a = hash(i);
  float b = hash(i + float2(1.0, 0.0));
  float c = hash(i + float2(0.0, 1.0));
  float d = hash(i + float2(1.0, 1.0));

  return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float3 water_normal(float2 world_xz, float time) {
  float epsilon = 0.5;

  float2 p = world_xz;

  // Sample noise at 3 octaves with scrolling offsets
  float2 scroll1 = float2(time * 0.3, time * 0.1);
  float2 scroll2 = float2(-time * 0.15, time * 0.25);
  float2 scroll3 = float2(time * 0.08, -time * 0.18);

  float h_c = noise((p + scroll1) * 0.12) * 0.5 + noise((p + scroll2) * 0.07) * 0.3 + noise((p + scroll3) * 0.19) * 0.2;
  float h_px = noise((p + float2(epsilon, 0.0) + scroll1) * 0.12) * 0.5 + noise((p + float2(epsilon, 0.0) + scroll2) * 0.07) * 0.3 + noise((p + float2(epsilon, 0.0) + scroll3) * 0.19) * 0.2;
  float h_pz = noise((p + float2(0.0, epsilon) + scroll1) * 0.12) * 0.5 + noise((p + float2(0.0, epsilon) + scroll2) * 0.07) * 0.3 + noise((p + float2(0.0, epsilon) + scroll3) * 0.19) * 0.2;

  float dx = (h_px - h_c) / epsilon;
  float dz = (h_pz - h_c) / epsilon;

  return normalize(float3(-dx * 0.8, 1.0, -dz * 0.8));
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float time = scene.time;

  float3 normal = water_normal(input.world_xz, time);

  float3 view_direction = normalize(scene.camera_position.xyz - input.position);
  float fresnel = pow(1.0 - max(dot(view_direction, normal), 0.0), 3.0);

  static const float3 SHALLOW_COLOR = float3(0.20, 0.40, 0.50);
  static const float3 DEEP_COLOR = float3(0.05, 0.15, 0.30);

  float3 water_color = lerp(SHALLOW_COLOR, DEEP_COLOR, fresnel);

  float3 sun_direction = normalize(scene.light_direction.xyz);
  float3 half_vector = normalize(view_direction + sun_direction);
  float specular = pow(max(dot(normal, half_vector), 0.0), 128.0) * 0.6;

  float ndotl = max(dot(normal, sun_direction), 0.0);
  float3 lit_color = water_color * (0.4 + 0.6 * ndotl) + float3(specular, specular, specular);

  float alpha = clamp(lerp(0.55, 0.85, fresnel), 0.01, 0.99);

  float w_alpha = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
  float w_depth = pow(1.0 - input.sv_position.z * 0.9, 3.0);
  float weight = clamp(w_alpha * 1e8 * w_depth, 1e-2, 3e3);

  output.accum = float4(lit_color * alpha, alpha) * weight;
  output.revealage = alpha;

  return output;
}
