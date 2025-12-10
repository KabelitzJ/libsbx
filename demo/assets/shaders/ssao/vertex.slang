struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct VSIn

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
vs_out main(vs_in in) {
  vs_out out;

  float2 position = positions[in.vertex_id];

  out.uv = (position + 1.0) * 0.5;
  out.position = float4(position, 0.0, 1.0);

  return out;
}