struct fs_in {
  float4 color : TEXCOORD0;
}; // struct ps_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct ps_out

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  output.color = input.color;

  return output;
}
