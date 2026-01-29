#include <libsbx/common/vertex.slang>
#include <libsbx/common/scene_data.slang>

struct transform_data {
  float4x4 model;
  float4x4 normal;
}; // struct transform_data

struct instance_data {
  uint transform_index;
  uint material_index;
  uint object_id; 
  uint _pad0;
}; // struct instance_data

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
  float tangent_w; // sign of the tangent
}; // struct vertex

struct push_data {
  vertex* vertex_buffer;
  transform_data* transform_data_buffer;
  instance_data* instance_data_buffer;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

struct vs_in {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
  uint base_vertex : SV_StartVertexLocation;
  uint base_instance : SV_StartInstanceLocation;
}; // struct vs_in

struct vs_out {
  float4 sv_position : SV_Position;
}; // struct vs_out

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  uint global_vertex = input.vertex_id + input.base_vertex;
  uint global_instance = input.instance_id + input.base_instance;

  instance_data instance_data = push.instance_data_buffer[global_instance];

  transform_data transform_data = push.transform_data_buffer[instance_data.transform_index];

  vertex vertex = push.vertex_buffer[global_vertex];

  float3 in_position = float3(vertex.position_x, vertex.position_y, vertex.position_z);
  float3 world_position = mul(transform_data.model, float4(in_position, 1.0)).xyz;

  output.sv_position = mul(scene.light_space, float4(world_position, 1.0));

  return output;
}
