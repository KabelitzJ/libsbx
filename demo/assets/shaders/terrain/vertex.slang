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

// IMPORTANT: matches your CPU layout exactly
struct instance_data {
  float red;
  float green;
  float blue;
  float height;

  uint transform_index;
  uint grid_quad_index;
  uint rotation_steps;
  uint padding0;

}; // struct instance_data

struct push_data {
  vertex* vertex_buffer;
  grid_vertex_data* grid_vertex_data_buffer;
  grid_quad_data* grid_quad_data_buffer;
  transform_data* transform_data_buffer;
  instance_data* instance_data_buffer;

  // Local-space bounds for the currently drawn mesh (per draw-call).
  // Use float4 to avoid alignment pitfalls in push constants.
  float4 mesh_bounds_min;
  float4 mesh_bounds_max;

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

static void quad_tangents(float3 a, float3 b, float3 c, float3 d, float s, float t, out float3 ds, out float3 dt) {
  ds = lerp(b - a, c - d, t);
  dt = lerp(d - a, c - b, s);
}

static float2 rotate_st(float2 st, uint steps) {
  uint r = steps & 3u;

  if (r == 0u) {
    return st;
  }

  if (r == 1u) {
    return float2(1.0f - st.y, st.x);
  }

  if (r == 2u) {
    return float2(1.0f - st.x, 1.0f - st.y);
  }

  return float2(st.y, 1.0f - st.x);
}

static float2 rotate_xz(float2 xz, uint steps) {
  uint r = steps & 3u;

  if (r == 0u) {
    return xz;
  }

  if (r == 1u) {
    return float2(-xz.y, xz.x);
  }

  if (r == 2u) {
    return float2(-xz.x, -xz.y);
  }

  return float2(xz.y, -xz.x);
}

static float safe_rcp(float v) {
  if (abs(v) < 1e-7f) {
    return 0.0f;
  }

  return 1.0f / v;
}

static float3 normalize_in_aabb(float3 p, float3 bmin, float3 bmax) {
  float3 ext = bmax - bmin;

  float inv_x = safe_rcp(ext.x);
  float inv_y = safe_rcp(ext.y);
  float inv_z = safe_rcp(ext.z);

  float3 uvw = float3(
    (p.x - bmin.x) * inv_x,
    (p.y - bmin.y) * inv_y,
    (p.z - bmin.z) * inv_z
  );

  return saturate(uvw);
}

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  uint global_vertex = input.vertex_id + input.base_vertex;
  uint global_instance = input.instance_id + input.base_instance;

  vertex v = push.vertex_buffer[global_vertex];
  instance_data inst = push.instance_data_buffer[global_instance];

  transform_data xf = push.transform_data_buffer[inst.transform_index];

  grid_quad_data quad = push.grid_quad_data_buffer[inst.grid_quad_index];

  float3 position_a = push.grid_vertex_data_buffer[quad.a].position;
  float3 position_b = push.grid_vertex_data_buffer[quad.b].position;
  float3 position_c = push.grid_vertex_data_buffer[quad.c].position;
  float3 position_d = push.grid_vertex_data_buffer[quad.d].position;

  float3 in_position = float3(v.position_x, v.position_y, v.position_z);
  float3 in_normal = float3(v.normal_x, v.normal_y, v.normal_z);
  float2 in_uv = float2(v.uv_x, v.uv_y);

  float3 bounds_min = push.mesh_bounds_min.xyz;
  float3 bounds_max = push.mesh_bounds_max.xyz;

  // ------------------------------------------------------------
  // FFD-style: normalize local position into AABB coordinates
  // uvw in [0,1]^3 describes the vertex as a percentage in the box
  // ------------------------------------------------------------
  float3 uvw = normalize_in_aabb(in_position, bounds_min, bounds_max);

  // ------------------------------------------------------------
  // Quad parameterization in XZ + rotation
  // ------------------------------------------------------------
  float2 st = float2(uvw.x, uvw.z);
  st = rotate_st(st, inst.rotation_steps);

  float s = st.x;
  float t = st.y;

  // ------------------------------------------------------------
  // Base point on the warped quad
  // ------------------------------------------------------------
  float3 base = quad_bilerp(position_a, position_b, position_c, position_d, s, t);

  // Surface tangents
  float3 ds;
  float3 dt;

  quad_tangents(position_a, position_b, position_c, position_d, s, t, ds, dt);

  float3 face_normal = normalize(cross(ds, dt));

  if (face_normal.y < 0.0f) {
    face_normal = -face_normal;
  }

  // ------------------------------------------------------------
  // Extrude using "u" percentage inside the AABB
  // ------------------------------------------------------------
  float u = uvw.y;
  float height = u * inst.height;

  float3 world_position = base + face_normal * height;

  // ------------------------------------------------------------
  // Normal: rotate local normal XZ, then remap into quad basis
  // ------------------------------------------------------------
  float2 n_xz = rotate_xz(float2(in_normal.x, in_normal.z), inst.rotation_steps);
  float3 local_normal = float3(n_xz.x, in_normal.y, n_xz.y);

  float3 ts = normalize(ds);
  float3 tt = normalize(dt);

  float3 world_normal = normalize(
    local_normal.x * ts +
    local_normal.z * tt +
    local_normal.y * face_normal
  );

  // ------------------------------------------------------------
  // Apply per-instance transform (optional but future-proof)
  // ------------------------------------------------------------
  float4 wp4 = mul(xf.model, float4(world_position, 1.0f));
  world_position = wp4.xyz;

  float3 wn = mul((float3x3)xf.normal, world_normal);
  world_normal = normalize(wn);

  // Rotate UVs too so textures match the tile rotation.
  float2 out_uv = rotate_st(in_uv, inst.rotation_steps);

  output.position = world_position;
  output.normal = world_normal;
  output.uv = out_uv;
  output.color = float3(inst.red, inst.green, inst.blue);

  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));

  return output;
}
