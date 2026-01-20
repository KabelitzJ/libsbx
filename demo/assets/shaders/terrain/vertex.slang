#include <libsbx/common/vertex.slang>
#include <libsbx/common/convert_to.slang>
#include <libsbx/common/scene_data.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
  uint base_vertex : SV_StartVertexLocation;
  uint base_instance : SV_StartInstanceLocation;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(3)]] float2 uv : UV;
  [[vk::location(4)]] float3 color : COLOR;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct vertex {
  float position_x;
  float position_y;
  float position_z;

  float normal_x;
  float normal_y;
  float normal_z;

  float uv_x;
  float uv_y;

  float tangent_x;
  float tangent_y;
  float tangent_z;
  float tangent_w;
}; // struct vertex

struct transform_data {
  float4x4 model;
  float4x4 normal;
}; // struct transform_data

struct grid_vertex_data {
  float3 position;
  uint flags;
}; // struct grid_vertex_data

struct grid_quad_data {
  uint a;
  uint b;
  uint c;
  uint d;
}; // struct grid_quad_data

struct instance_data {
  uint transform_index;
  uint grid_quad_index;
  float height;
  float red;

  float green;
  float blue;
  uint rotation_steps;
  uint padding;
}; // struct instance_data

struct push_data {
  vertex* vertex_buffer;
  grid_vertex_data* grid_vertex_data_buffer;
  grid_quad_data* grid_quad_data_buffer;
  transform_data* transform_data_buffer;
  instance_data* instance_data_buffer;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

// Bilinear on quad corners (a,b,c,d) with
// (0,0)=a, (1,0)=b, (1,1)=c, (0,1)=d
static float3 quad_bilerp(float3 a, float3 b, float3 c, float3 d, float s, float t) {
  float3 p0 = lerp(a, b, s);
  float3 p1 = lerp(d, c, s);
  return lerp(p0, p1, t);
}

// Tangents of the bilerp surface (planar here, but robust)
static void quad_tangents(float3 a, float3 b, float3 c, float3 d, float s, float t, out float3 ds, out float3 dt) {
  // p(s,t) = lerp( lerp(a,b,s), lerp(d,c,s), t )
  // dp/ds = lerp(b-a, c-d, t)
  // dp/dt = lerp(d-a, c-b, s)
  ds = lerp(b - a, c - d, t);
  dt = lerp(d - a, c - b, s);
}

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  uint global_vertex = input.vertex_id + input.base_vertex;
  uint global_instance = input.instance_id + input.base_instance;

  vertex vertex = push.vertex_buffer[global_vertex];
  instance_data instance_data = push.instance_data_buffer[global_instance];

  // (we keep transform_data around for future use, but we won't apply its XZ transform here)
  transform_data transform_data = push.transform_data_buffer[instance_data.transform_index];

  grid_quad_data grid_quad_data = push.grid_quad_data_buffer[instance_data.grid_quad_index];

  float3 position_a = push.grid_vertex_data_buffer[grid_quad_data.a].position;
  float3 position_b = push.grid_vertex_data_buffer[grid_quad_data.b].position;
  float3 position_c = push.grid_vertex_data_buffer[grid_quad_data.c].position;
  float3 position_d = push.grid_vertex_data_buffer[grid_quad_data.d].position;

  float3 in_position = float3(vertex.position_x, vertex.position_y, vertex.position_z);
  float3 in_normal = float3(vertex.normal_x, vertex.normal_y, vertex.normal_z);
  float2 in_uv = float2(vertex.uv_x, vertex.uv_y);

  float2 uv = in_uv;
  uint r = instance_data.rotation_steps & 3;

  if (r != 0) {
    // move to center
    uv -= float2(0.5, 0.5);

    if (r == 1)      uv = float2(-uv.y,  uv.x); // 90°
    else if (r == 2) uv = float2(-uv.x, -uv.y); // 180°
    else if (r == 3) uv = float2( uv.y, -uv.x); // 270°

    // move back
    uv += float2(0.5, 0.5);
  }

  float3 warped_position = quad_bilerp(position_a, position_b, position_c, position_d, uv.x, uv.y);
  // warped_position.y += instance_data.height;

  float3 ds, dt;
  quad_tangents(position_a, position_b, position_c, position_d, uv.x, uv.y, ds, dt);

  float3 warped_normal = normalize(cross(dt, ds));

  output.position = warped_position;
  output.normal = warped_normal;
  output.uv = uv;
  output.color = float3(instance_data.red, instance_data.green, instance_data.blue);

  output.sv_position = mul(scene.projection, mul(scene.view, float4(warped_position, 1.0)));

  return output;
}
