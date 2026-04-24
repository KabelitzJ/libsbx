struct glyph {
  float2 offset;
  float2 size;
  float2 uv_offset;
  float2 uv_size;
}; // struct glyph

struct vs_in {
  [[vk::location(0)]] float2 in_position : POSITION;
  [[vk::location(1)]] float2 in_uv : UV;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float2 out_uv : UV;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct uniform_scene {
  float4x4 projection;
}; // struct uniform_scene

[[vk::binding(0, 0)]]
ConstantBuffer<uniform_scene> scene;

struct glyph_buffer {
  glyph data[];
}; // struct glyph_buffer

[[vk::binding(1, 0)]]
StructuredBuffer<glyph> glyphs;

float2 calculate_uv(glyph glyph, float2 in_uv) {
  return glyph.uv_offset + in_uv * glyph.uv_size;
}

float2 calculate_position(glyph glyph, float2 in_position) {
  return glyph.offset + in_position * glyph.size;
}

[shader("vertex")]
vs_out main(vs_in input, uint instance_id : SV_InstanceID)  {
  vs_out output;

  glyph g = glyphs[instance_id];

  output.out_uv = calculate_uv(g, input.in_uv);

  float2 position = calculate_position(g, input.in_position);

  output.sv_position = mul(scene.projection, float4(position, 0.0f, 1.0f));

  return output;
}
