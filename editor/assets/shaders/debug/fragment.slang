struct fs_in {
  float4 color : TEXCOORD0;
}; // struct ps_in

struct fs_out {
  float4 color : SV_Target0;
  float4 brightness : SV_Target1;
}; // struct ps_out

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  output.color = input.color;
  output.brightness = float4(0.0, 0.0, 0.0, 1.0);

  return output;
}
