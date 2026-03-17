#include <libsbx/common/scene_data.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 accum : SV_Target0;
  float revealage : SV_Target1;
}; // struct fs_out

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  static const float3 WATER_COLOR = float3(0.15, 0.30, 0.45);
  static const float WATER_ALPHA = 0.45;

  float3 sun_direction = normalize(scene.light_direction.xyz);
  float ndotl = max(dot(input.normal, sun_direction), 0.0);
  float3 lit_color = WATER_COLOR * (0.4 + 0.6 * ndotl);

  float alpha = clamp(WATER_ALPHA, 0.01, 0.99);

  float w_alpha = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
  float w_depth = pow(1.0 - input.sv_position.z * 0.9, 3.0);

  float weight = clamp(w_alpha * 1e8 * w_depth, 1e-2, 3e3);

  output.accum = float4(lit_color * alpha, alpha) * weight;
  output.revealage = alpha;

  return output;
}
