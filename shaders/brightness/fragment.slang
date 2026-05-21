struct fs_in {
  float2 uv : TEXCOORD0;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

[[vk::binding(0, 0)]]
Sampler2D source_image;

struct push_data {
  float threshold;
  float soft_knee;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

float3 soft_knee_bright_pass(float3 color, float threshold, float soft_knee) {
  float brightness = max(max(color.r, color.g), color.b);

  float knee = threshold * soft_knee;
  float soft = brightness - threshold + knee;
  soft = clamp(soft, 0.0, 2.0 * knee);
  soft = soft * soft / (4.0 * knee + 1e-4);

  float contribution = max(soft, brightness - threshold);
  contribution /= max(brightness, 1e-4);

  return color * contribution;
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float3 color = source_image.Sample(input.uv).rgb;

  color = min(color, 64.0);

  output.color = float4(soft_knee_bright_pass(color, push.threshold, push.soft_knee), 1.0);

  return output;
}
