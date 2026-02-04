#include <libsbx/common/scene_data.slang>
#include <libsbx/common/image.slang>

#include <particles/common.slang>

struct fs_in {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] float4 color : COLOR;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 accum : SV_Target0;
  float revealage : SV_Target1;
}; // struct fs_out

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(3, 0)]]
Sampler2D<float4> image;

[[vk::push_constant]]
ConstantBuffer<render_params> params;

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float4 sampled = image.Sample(input.uv);
  float4 albedo = sampled * input.color;

  if (albedo.a <= 0.001) {
    discard;
  }

  float alpha = clamp(albedo.a, 0.01, 0.99);

  float w_alpha = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
  float w_depth = pow(1.0 - input.sv_position.z * 0.9, 3.0);

  float weight = clamp(w_alpha * 1e8 * w_depth, 1e-2, 3e3);

  output.accum = float4(albedo.rgb * albedo.a, albedo.a) * weight;
  output.revealage = albedo.a;

  return output;
}
