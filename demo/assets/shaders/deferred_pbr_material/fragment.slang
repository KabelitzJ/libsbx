#include <libsbx/common/depth.slang>
#include <libsbx/common/image.slang>
#include <libsbx/common/scene_data.slang>
#include <libsbx/common/texture_sampler.slang>

#include <common.slang>
#include <alpha_policy.slang>

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
  [[vk::location(3)]] float2 uv : UV;
  [[vk::location(4)]] nointerpolation uint instance_index : INSTANCE_INDEX;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct opaque_fs_out {
  float4 albedo : SV_Target0;
  float4 position : SV_Target1;
  float4 normal : SV_Target2;
  float4 material : SV_Target3;
  float4 emissive : SV_Target4;
  uint object_id: SV_Target5;
  float depth : SV_Target6;
}; // struct opaque_fs_out

struct blend_fs_out {
  float4 accum : SV_Target0;
  float revealage : SV_Target1;
}; // struct blend_fs_out

[[vk::push_constant]]
ConstantBuffer<static_push_data> static_push;

[[vk::push_constant]]
ConstantBuffer<skinned_push_data> skinned_push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]]
SamplerState images_sampler;

[[vk::binding(2, 0)]]
Texture2D<float4> images[sbx::common::max_image_array_size];

float4 get_albedo<AlphaPolicy: alpha_policy>(float2 uv, material_data material_data) {
  uint index = material_data.albedo_index;

  if (index >= sbx::common::max_image_array_size) {
    return material_data.base_color;
  }

  float4 color = images[NonUniformResourceIndex(index)].Sample(images_sampler, uv);

  return AlphaPolicy::combine_colors(color, material_data.base_color);
}

float3 get_normal(float3 T, float3 B, float3 N, float2 uv, float3 normal, material_data material_data) {
  uint index = material_data.normal_index;

  if (index >= sbx::common::max_image_array_size) {
    return normalize(normal);
  }

  float3 sampled_normal = images[NonUniformResourceIndex(index)].Sample(images_sampler, uv).xyz;
  sampled_normal = sampled_normal * 2.0 - 1.0;
  sampled_normal.xy *= material_data.normal_scale;
  sampled_normal = normalize(sampled_normal);

  return normalize(T * sampled_normal.x + B * sampled_normal.y + N * sampled_normal.z);
}

float3 get_material(float2 uv, material_data material_data) {
  uint index = material_data.mrao_index;

  if (index >= sbx::common::max_image_array_size) {
    return float3(material_data.metallic, material_data.roughness, material_data.occlusion);
  }

  return images[NonUniformResourceIndex(index)].Sample(images_sampler, uv).rgb;
}

float3 get_emissive(float2 uv, material_data material_data) {
  uint index = material_data.emissive_index;

  float3 emissive = material_data.emissive_factor.rgb * material_data.emissive_strength;

  if (index >= sbx::common::max_image_array_size) {
    return emissive;
  }

  return emissive * images[NonUniformResourceIndex(index)].Sample(images_sampler, uv).rgb;
}

opaque_fs_out opaque_main(fs_in in, material_data material_data, uint object_id) {
  opaque_fs_out out;

  float3 N = normalize(in.normal);
  float3 T = normalize(in.tangent.xyz);
  T = normalize(T - N * dot(T, N));
  float3 B = normalize(cross(N, T)) * in.tangent.w;

  out.albedo = get_albedo<opaque_alpha_policy>(in.uv, material_data);
  out.position = float4(in.position, 1.0);
  out.normal = float4(get_normal(T, B, N, in.uv, in.normal, material_data), 0.0);
  out.material = float4(get_material(in.uv, material_data), 0.0);
  out.emissive = float4(get_emissive(in.uv, material_data), 0.0);
  out.object_id = object_id;
  out.depth = sbx::common::linearize_depth(in.sv_position.z, scene.camera_near, scene.camera_far);

  return out;
}

[earlydepthstencil]
[shader("fragment")]
opaque_fs_out static_opaque_main(fs_in in) {
  uint global_instance = in.instance_index;

  static_instance_data instance_data = static_push.instance_data_buffer[global_instance];
  material_data material_data = static_push.material_data_buffer[instance_data.material_index];

  return opaque_main(in, material_data, instance_data.object_id);
}

[earlydepthstencil]
[shader("fragment")]
opaque_fs_out skinned_opaque_main(fs_in in) {
  uint global_instance = in.instance_index;

  skinned_instance_data instance_data = skinned_push.instance_data_buffer[global_instance];
  material_data material_data = skinned_push.material_data_buffer[instance_data.material_index];

  return opaque_main(in, material_data, instance_data.object_id);
}

opaque_fs_out mask_main(fs_in in, material_data material_data, uint object_id) {
  opaque_fs_out out;

  float3 N = normalize(in.normal);
  float3 T = normalize(in.tangent.xyz);
  T = normalize(T - N * dot(T, N));
  float3 B = normalize(cross(N, T)) * in.tangent.w;

  float4 albedo = get_albedo<mask_alpha_policy>(in.uv, material_data);

  if (albedo.a < material_data.alpha_cutoff) {
    discard;
  }

  out.albedo = albedo;
  out.position = float4(in.position, 1.0);
  out.normal = float4(get_normal(T, B, N, in.uv, in.normal, material_data), 0.0);
  out.material = float4(get_material(in.uv, material_data), 0.0);
  out.emissive = float4(get_emissive(in.uv, material_data), 0.0);
  out.object_id = object_id;
  out.depth = sbx::common::linearize_depth(in.sv_position.z, scene.camera_near, scene.camera_far);

  return out;
}

[shader("fragment")]
opaque_fs_out static_mask_main(fs_in in) {
  uint global_instance = in.instance_index;

  static_instance_data instance_data = static_push.instance_data_buffer[global_instance];
  material_data material_data = static_push.material_data_buffer[instance_data.material_index];

  return mask_main(in, material_data, instance_data.object_id);
}

[shader("fragment")]
opaque_fs_out skinned_mask_main(fs_in in) {
  uint global_instance = in.instance_index;

  skinned_instance_data instance_data = skinned_push.instance_data_buffer[global_instance];
  material_data material_data = skinned_push.material_data_buffer[instance_data.material_index];

  return mask_main(in, material_data, instance_data.object_id);
}

blend_fs_out blend_main(fs_in in, material_data material_data, uint object_id) {
  blend_fs_out out;
  
  float4 albedo = get_albedo<blend_alpha_policy>(in.uv, material_data);

  // Equivalent to: clamp(pow(min(1, a*10)+0.01, 3)*1e8 * pow(1 - z*0.9, 3), 1e-2, 3e3)
  float alpha = albedo.a;

  float w_alpha = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
  float w_depth = pow(1.0 - in.sv_position.z * 0.9, 3.0);

  float weight = clamp(w_alpha * 1e8 * w_depth, 1e-2, 3e3);

  out.accum = float4(albedo.rgb * albedo.a, albedo.a) * weight;
  out.revealage = albedo.a;

  return out;
}

[shader("fragment")]
blend_fs_out static_blend_main(fs_in in) {
  uint global_instance = in.instance_index;

  static_instance_data instance_data = static_push.instance_data_buffer[global_instance];
  material_data material_data = static_push.material_data_buffer[instance_data.material_index];

  return blend_main(in, material_data, instance_data.object_id);
}

[shader("fragment")]
blend_fs_out skinned_blend_main(fs_in in) {
  uint global_instance = in.instance_index;

  skinned_instance_data instance_data = skinned_push.instance_data_buffer[global_instance];
  material_data material_data = skinned_push.material_data_buffer[instance_data.material_index];

  return blend_main(in, material_data, instance_data.object_id);
}
