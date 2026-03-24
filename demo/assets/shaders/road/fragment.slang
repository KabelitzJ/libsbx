#include <libsbx/common/depth.slang>
#include <libsbx/common/scene_data.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float edge_factor : EDGE_FACTOR;
  [[vk::location(2)]] nointerpolation uint road_type : ROAD_TYPE;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 albedo : SV_Target0;
  float4 position : SV_Target1;
  float4 normal : SV_Target2;
  float4 material : SV_Target3;
  float4 emissive : SV_Target4;
  uint object_id : SV_Target5;
  float linear_depth : SV_Target6;
}; // struct fs_out

struct road_vertex_data {
  float position_x;
  float position_y;
  float position_z;
  float edge_factor;
  uint road_type;
}; // struct road_vertex_data

struct push_data {
  road_vertex_data* vertex_data_buffer;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

static const float3 COLOR_DIRT    = float3(0.45, 0.38, 0.28);
static const float3 COLOR_GRAVEL  = float3(0.55, 0.52, 0.48);
static const float3 COLOR_PAVED   = float3(0.25, 0.25, 0.27);
static const float3 COLOR_HIGHWAY = float3(0.20, 0.20, 0.22);

float3 get_road_color(uint type) {
  if (type == 1) return COLOR_DIRT;
  if (type == 2) return COLOR_GRAVEL;
  if (type == 3) return COLOR_PAVED;
  if (type == 4) return COLOR_HIGHWAY;
  return float3(1.0, 0.0, 1.0);
}

float get_road_roughness(uint type) {
  if (type == 1) return 0.95;
  if (type == 2) return 0.85;
  if (type == 3) return 0.55;
  if (type == 4) return 0.45;
  return 1.0;
}

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  // Fade edges to transparent — discard fully faded pixels
  float edge_blend = smoothstep(0.6, 1.0, input.edge_factor);

  if (edge_blend > 0.99) {
    discard;
  }

  float3 road_color = get_road_color(input.road_type);
  float road_roughness = get_road_roughness(input.road_type);

  // Darken edges for a worn-in look
  float3 edge_tint = road_color * 0.7;
  float3 albedo = lerp(road_color, edge_tint, input.edge_factor * 0.4);

  // Compute face normal from screen-space derivatives
  float3 dx = ddx(input.position);
  float3 dy = ddy(input.position);
  float3 normal = normalize(cross(dx, dy));

  // Ensure normal points upward
  if (normal.y < 0.0) {
    normal = -normal;
  }

  output.albedo = float4(albedo, 1.0);
  output.position = float4(input.position, 1.0);
  output.normal = float4(normal * 0.5 + 0.5, 1.0);
  output.material = float4(0.0, road_roughness, 1.0, 1.0);
  output.emissive = float4(0.0, 0.0, 0.0, 1.0);
  output.object_id = 0;
  output.linear_depth = sbx::common::linearize_depth(input.sv_position.z, scene.camera_near, scene.camera_far);

  return output;
}
