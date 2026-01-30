#define MAX_IMAGE_ARRAY_SIZE 64

struct fs_in {
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float color : SV_Target0;
}; // struct fs_out

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  output.color = float(input.sv_position.z);

  return output;
}
