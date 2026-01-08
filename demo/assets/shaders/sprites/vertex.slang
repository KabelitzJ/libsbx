#include <libsbx/common/scene_data.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] float4 color : COLOR;
  [[vk::location(2)]] nointerpolation uint image_index : IMAGE_INDEX;
  float4 sv_position : SV_Position;
}; // struct vs_out

enum sprite_space {
  screen_overlay,
  screen_camera,
  world
}; // enum sprite_space

struct sprite_data {
  float4x4 model;
  float4 color;
  float2 size;
  uint space;
  uint image_index;
}; // struct sprite_data

struct push_data {
  Ptr<sprite_data> sprites;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

static const float3 vertices[4] = {
  float3(-1.0, -1.0, 0.0),
  float3( 1.0, -1.0, 0.0),
  float3( 1.0,  1.0, 0.0),
  float3(-1.0,  1.0, 0.0)
};

static const int indices[6] = { 0, 1, 2, 2, 3, 0 };

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  sprite_data sprite = push.sprites[input.instance_id];

  int index = indices[input.vertex_id];

  float3 base_position = vertices[index];

  float3 position = base_position;
  position.x *= sprite.size.x * 0.5;
  position.y *= sprite.size.y * 0.5;

  float4x4 mv = mul(scene.view, sprite.model);

  mv[0][0] = 1.0; 
  mv[0][1] = 0.0; 
  mv[0][2] = 0.0; 

  mv[1][0] = 0.0; 
  mv[1][1] = 1.0; 
  mv[1][2] = 0.0; 

  mv[2][0] = 0.0; 
  mv[2][1] = 0.0; 
  mv[2][2] = 1.0; 

  // float3 world_position = mul(sprite.model, float4(position, 1.0)).xyz;

  output.uv = base_position.xy * 0.5 + 0.5;
  output.color = sprite.color;
  output.image_index = sprite.image_index;

  output.sv_position = mul(scene.projection, mul(mv, float4(position, 1.0)));

  return output;
}
