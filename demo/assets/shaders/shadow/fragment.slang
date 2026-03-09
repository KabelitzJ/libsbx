#include <libsbx/common/depth.slang>
#include <libsbx/common/image.slang>
#include <libsbx/common/scene_data.slang>

#include <shadow/common.slang>
#include <shadow/alpha_policy.slang>

struct fs_in {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] nointerpolation uint instance_index : INSTANCE_INDEX;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float color : SV_Target0;
}; // struct fs_out

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(0, 1)]]
SamplerState samplers[sbx::common::max_sampler_array_size];

[[vk::binding(0, 2)]]
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

[noinline] T maybe_unused<T>(T value) {
  return value;
}

[earlydepthstencil]
[shader("fragment")]
fs_out opaque_main(fs_in input) {
  fs_out output;

  maybe_unused(images);
  maybe_unused(samplers);
  
  uint global_instance = input.instance_index;

  instance_data instance_data = push.instance_data_buffer[global_instance];
  material_data material_data = push.material_data_buffer[instance_data.material_index];

  output.color = float(input.sv_position.z);

  return output;
}

[shader("fragment")]
fs_out mask_main(fs_in input) {
  fs_out output;
  
  uint global_instance = input.instance_index;

  instance_data instance_data = push.instance_data_buffer[global_instance];
  material_data material_data = push.material_data_buffer[instance_data.material_index];

  float2 uv = input.uv * material_data.uv_scale + material_data.uv_offset;

  float4 albedo = get_albedo<mask_alpha_policy>(uv, material_data);

  clip(albedo.a - material_data.alpha_cutoff);

  output.color = float(input.sv_position.z);

  return output;
}
