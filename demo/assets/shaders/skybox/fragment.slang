struct fs_in {
  float3 uv : TEXCOORD0;
  float4 color : TEXCOORD1;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
  float4 brightness : SV_Target1;
}; // struct fs_out

[[vk::binding(1, 0)]]
SamplerCube<float4> skybox;

static const float FOG_DISTANCE = 400.0;
static const float FOG_DENSITY = 0.002;
static const float4 FOG_COLOR = float4(0.5, 0.5, 0.5, 1.0);

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  float3 direction = normalize(input.uv);
  float4 cube_map_color = skybox.Sample(direction) * input.color;

  float fog_factor = clamp(1.0 - exp(-FOG_DISTANCE * FOG_DENSITY), 0.0, 1.0);

  output.color = cube_map_color; // lerp(cube_map_color, FOG_COLOR, fog_factor);
  output.brightness = float4(0.0, 0.0, 0.0, 1.0);

  return output;
}
