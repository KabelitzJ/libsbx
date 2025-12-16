struct vertex {
  float4 position;
  float4 color;
}; // struct vertex

struct push_data {
  float4x4 mvp;
  Ptr<vertex> vertices;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

struct vs_in { 
  uint vertex_id : SV_VertexID; 
}; // struct vs_in

struct vs_out {
  float4 color : TEXCOORD0; 
  float4 position : SV_Position;
}; // struct vs_out

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  vertex vertex = push.vertices[input.vertex_id];

  output.color = vertex.color;
  output.position = mul(push.mvp, vertex.position);

  return output;
}