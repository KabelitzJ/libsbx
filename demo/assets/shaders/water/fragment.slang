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

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

// Cheap scrolling normal perturbation using layered sine waves
float3 water_normal(float2 world_xz, float time) {
  // Two layers scrolling in different directions
  float2 uv1 = world_xz * 0.15 + float2(time * 0.3, time * 0.1);
  float2 uv2 = world_xz * 0.08 + float2(-time * 0.15, time * 0.25);

  // Derive normal perturbation from sine waves
  float nx1 = cos(uv1.x * 6.28) * sin(uv1.y * 6.28) * 0.15;
  float nz1 = sin(uv1.x * 6.28) * cos(uv1.y * 6.28) * 0.15;

  float nx2 = cos(uv2.x * 4.0) * sin(uv2.y * 4.0) * 0.1;
  float nz2 = sin(uv2.x * 4.0) * cos(uv2.y * 4.0) * 0.1;

  return normalize(float3(nx1 + nx2, 1.0, nz1 + nz2));
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float time = scene.time;

  // Animated normal for rippling light
  float3 normal = water_normal(input.world_xz, time);

  // Fresnel — more transparent looking straight down, more opaque at grazing angles
  float3 view_direction = normalize(scene.camera_position.xyz - input.position);
  float fresnel = pow(1.0 - max(dot(view_direction, normal), 0.0), 3.0);

  // Water color — deeper blue at steep view angles
  static const float3 SHALLOW_COLOR = float3(0.20, 0.40, 0.50);
  static const float3 DEEP_COLOR = float3(0.05, 0.15, 0.30);

  float3 water_color = lerp(SHALLOW_COLOR, DEEP_COLOR, fresnel);

  // Sun specular highlight
  float3 sun_direction = normalize(scene.light_direction.xyz);
  float3 half_vector = normalize(view_direction + sun_direction);
  float specular = pow(max(dot(normal, half_vector), 0.0), 128.0) * 0.6;

  // Diffuse lighting
  float ndotl = max(dot(normal, sun_direction), 0.0);
  float3 lit_color = water_color * (0.4 + 0.6 * ndotl) + float3(specular, specular, specular);

  // Alpha varies with fresnel — more opaque at grazing angles
  float alpha = clamp(lerp(0.35, 0.75, fresnel), 0.01, 0.99);

  // Weighted blended OIT — same formula as your static mesh transparency
  float w_alpha = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
  float w_depth = pow(1.0 - input.sv_position.z * 0.9, 3.0);
  float weight = clamp(w_alpha * 1e8 * w_depth, 1e-2, 3e3);

  output.accum = float4(lit_color * alpha, alpha) * weight;
  output.revealage = alpha;

  return output;
}
