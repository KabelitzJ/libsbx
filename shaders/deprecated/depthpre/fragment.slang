#include <libsbx/common/image.slang>
#include <libsbx/common/scene_data.slang>

#include <common.slang>
#include <alpha_policy.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
  [[vk::location(3)]] float2 uv : UV;
  [[vk::location(4)]] nointerpolation uint instance_index : INSTANCE_INDEX;
  float4 sv_position : SV_Position;
  bool sv_is_front_face: SV_IsFrontFace;
}; // struct fs_in

struct depth_fs_out {
  float depth : SV_Depth;
}; // struct depth_fs_out

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]]
SamplerState samplers[sbx::common::max_image_array_size];

[[vk::binding(2, 0)]]
Texture2D<float4> images[sbx::common::max_image_array_size];

float4 get_albedo<AlphaPolicy: alpha_policy>(float2 uv, material_data material_data) {
  uint image_index = material_data.albedo_image_index;
  uint sampler_index = material_data.albedo_sampler_index;

  if (image_index >= sbx::common::max_image_array_size) {
    return material_data.base_color;
  }

  float4 color = images[NonUniformResourceIndex(image_index)].Sample(samplers[NonUniformResourceIndex(sampler_index)], uv);

  return AlphaPolicy::combine_colors(color, material_data.base_color);
}

[noinline] T keep<T>(T value) {
  return value;
}

[earlydepthstencil]
[shader("fragment")]
depth_fs_out opaque_main(fs_in input) {
  depth_fs_out output;

  keep(push);
  keep(scene);
  keep(samplers);
  keep(images);

  output.depth = input.sv_position.z;

  return output;
}

[shader("fragment")]
depth_fs_out  mask_main(fs_in input) {
  depth_fs_out output;

  keep(push);
  keep(scene);
  keep(samplers);
  keep(images);

  uint global_instance = input.instance_index;
  
  instance_data instance = push.instance_data_buffer[global_instance];
  material_data material = push.material_data_buffer[instance.material_index];

  float2 uv = input.uv * material.uv_scale + material.uv_offset;

  float4 albedo = get_albedo<mask_alpha_policy>(uv, material);

  clip(albedo.a - material.alpha_cutoff);

  output.depth = input.sv_position.z;

  return output;
}
