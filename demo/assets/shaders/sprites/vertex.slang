#include <libsbx/common/scene_data.slang>

#include <sprites/common.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] nointerpolation uint instance_id : INSTANCE_ID;
  float4 sv_position : SV_Position;
}; // struct vs_out

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

interface render_mode {
  static float4 compute_position(float3 local_position, sprite_data sprite);
}; // interface render_mode

struct screen_overlay_mode : render_mode {
  static float4 compute_position(float3 local_position, sprite_data sprite) {
    float2 screen_pos = sprite.model[3].xy;
    
    float2 pivot_offset = (sprite.pivot - 0.5) * 2.0;
    float2 position_2d = local_position.xy - pivot_offset;
    
    position_2d.x *= sprite.size.x * 0.5;
    position_2d.y *= sprite.size.y * 0.5;
    
    position_2d += screen_pos;
    
    float2 ndc = position_2d / (scene.viewport * 0.5);
    ndc.y = -ndc.y; // Flip Y for screen coordinates
    
    return float4(ndc, 0.0, 1.0);
  }
}; // struct screen_overlay_mode

struct screen_camera_mode : render_mode {
  static float4 compute_position(float3 local_position, sprite_data sprite) {
    float3 sprite_pos = sprite.model[3].xyz;
    
    float2 pivot_offset = (sprite.pivot - 0.5) * 2.0;
    float2 position_2d = local_position.xy - pivot_offset;
    
    position_2d.x *= sprite.size.x * 0.5;
    position_2d.y *= sprite.size.y * 0.5;
    
    position_2d += sprite_pos.xy;
    
    float2 ndc = position_2d / (scene.viewport * 0.5);
    ndc.y = -ndc.y;
    
    float depth = saturate((sprite_pos.z - scene.camera_near) / (scene.camera_far - scene.camera_near));
    
    return float4(ndc, depth, 1.0);
  }
}; // struct screen_camera_mode

struct world_space_mode : render_mode {
  static float4 compute_position(float3 local_position, sprite_data sprite) {
    float2 pivot_offset = (sprite.pivot - 0.5) * 2.0;
    float3 position = local_position;
    position.xy -= pivot_offset;
    
    position.x *= sprite.size.x * 0.5;
    position.y *= sprite.size.y * 0.5;
    
    float4x4 mv = mul(scene.view, sprite.model);
    
    if ((sprite.flags & flag_billboard) != 0) {
      float scale_x = length(float3(mv[0][0], mv[1][0], mv[2][0]));
      float scale_y = length(float3(mv[0][1], mv[1][1], mv[2][1]));
      float scale_z = length(float3(mv[0][2], mv[1][2], mv[2][2]));
      
      mv[0][0] = scale_x;
      mv[1][0] = 0.0;
      mv[2][0] = 0.0;

      mv[0][1] = 0.0;
      mv[1][1] = scale_y;
      mv[2][1] = 0.0;

      mv[0][2] = 0.0;
      mv[1][2] = 0.0;
      mv[2][2] = scale_z;
    }
    
    return mul(scene.projection, mul(mv, float4(position, 1.0)));
  }
}; // struct world_space_mode


[shader("vertex")]
vs_out main<Mode : render_mode>(vs_in input) {
  vs_out output;

  sprite_data sprite = push.sprites[input.instance_id];

  int index = indices[input.vertex_id];
  float3 base_position = vertices[index];

  output.sv_position = Mode::compute_position(base_position, sprite);
  output.uv = base_position.xy * 0.5 + 0.5;
  output.instance_id = input.instance_id;

  return output;
}