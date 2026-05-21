struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
}; // struct vs_out

static const float2 positions[3] = {
  float2(-1.0, -1.0),
  float2( 3.0, -1.0),
  float2(-1.0,  3.0)
};

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  float2 position = positions[input.vertex_id];

  output.uv = (position + 1.0) * 0.5;
  output.position = float4(position, 0.0, 1.0);

  return output;
}