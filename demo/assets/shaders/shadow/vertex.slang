#include <libsbx/common/vertex.slang>
#include <libsbx/common/scene_data.slang>

struct transform_data {
  float4x4 model;
  float4x4 normal;
}; // struct transform_data

struct instance_data {
  float4 tint;
  float4 material;
  uint4 payload;
  uint4 selection;
}; // struct instance_data

struct vertex : sbx::common::position<3>, sbx::common::normal<3>, sbx::common::tangent<4>, sbx::common::uv<2> {
  float position_x;
  float position_y;
  float position_z;
  float normal_x;
  float normal_y;
  float normal_z;
  float tangent_x;
  float tangent_y;
  float tangent_z;
  float tangent_w;
  float uv_x;
  float uv_y;

  vector<float,3> get_position() {
    return vector<float,3>(position_x, position_y, position_z);
  }

  vector<float,3> get_normal() {
    return vector<float,3>(normal_x, normal_y, normal_z);
  }

  vector<float,4> get_tangent() {
    return vector<float,4>(tangent_x, tangent_y, tangent_z, tangent_w);
  }

  vector<float,2> get_uv() {
    return vector<float,2>(uv_x, uv_y);
  }
}; // struct vertex

struct push_data {
  Ptr<vertex> vertex_buffer;
  Ptr<transform_data> transform_data_buffer;
  Ptr<instance_data> instance_data_buffer;
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

  uint transform_data_index = uint(instance_data.payload.w);

  transform_data transform_data = push.transform_data_buffer[transform_data_index];

  vertex vertex = push.vertex_buffer[global_vertex];

  float3 in_position = sbx::common::get_position(vertex);
  float3 world_position = mul(transform_data.model, float4(in_position, 1.0)).xyz;

  output.sv_position = mul(scene.light_space, float4(world_position, 1.0));

  return output;
}
