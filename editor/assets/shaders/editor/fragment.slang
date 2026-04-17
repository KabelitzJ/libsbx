struct fs_in {
  [[vk::location(0)]] float4 Color : TEXCOORD0;
  [[vk::location(1)]] float2 UV : TEXCOORD1;
}; // struct fs_in

struct fs_out {
  float4 fColor : SV_Target0;
}; // struct fs_out

[[vk::binding(0, 0)]]
Sampler2D<float4> sTexture;

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  output.fColor = input.Color * sTexture.Sample(input.UV);

  return output;
}
