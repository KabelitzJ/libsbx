struct fs_in {
  [[vk::location(0)]] float2 in_uv : UV;
}; // struct fs_in

struct fs_out {
  float4 out_color : SV_Target0;
}; // struct fs_out

struct uniform_object {
  float4 color;
}; // struct uniform_object

[[vk::binding(2, 0)]]
ConstantBuffer<uniform_object> object;

[[vk::binding(3, 0)]]
Sampler2D<float4> atlas;

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float opacity = atlas.Sample(input.in_uv).r;

  output.out_color = float4(1.0f, 1.0f, 1.0f, opacity) * object.color;

  return output;
}
