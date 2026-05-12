#include <libsbx/common/depth.slang>
#include <libsbx/common/image.slang>
#include <libsbx/common/scene_data.slang>

#include <deferred_pbr_material/common.slang>
#include <deferred_pbr_material/alpha_policy.slang>

struct gbuffer_fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
  [[vk::location(3)]] float2 uv : UV;
  [[vk::location(4)]] nointerpolation uint instance_index : INSTANCE_INDEX;
#if defined(VERTEX_STREAM_COLOR)
  [[vk::location(5)]] float4 color : COLOR0;
#endif
#if defined(VERTEX_STREAM_CUSTOM0)
  [[vk::location(6)]] float4 custom0 : CUSTOM0;
#endif
#if defined(VERTEX_STREAM_CUSTOM1)
  [[vk::location(7)]] float4 custom1 : CUSTOM1;
#endif
#if defined(VERTEX_STREAM_CUSTOM2)
  [[vk::location(8)]] float4 custom2 : CUSTOM2;
#endif
#if defined(VERTEX_STREAM_CUSTOM3)
  [[vk::location(9)]] float4 custom3 : CUSTOM3;
#endif
  float4 sv_position : SV_Position;
  bool sv_is_front_face: SV_IsFrontFace;
}; // struct gbuffer_fs_in

struct shadow_fs_in {
  [[vk::location(0)]] float2 uv : UV;
  [[vk::location(1)]] nointerpolation uint instance_index : INSTANCE_INDEX;
  float4 sv_position : SV_Position;
}; // struct shadow_fs_in

struct opaque_fs_out {
  float4 albedo : SV_Target0;
  float4 position : SV_Target1;
  float4 normal : SV_Target2;
  float4 material : SV_Target3;
  float4 emissive : SV_Target4;
  uint object_id: SV_Target5;
  float linear_depth : SV_Target6;
}; // struct opaque_fs_out

struct blend_fs_out {
  float4 accumulator : SV_Target0;
  float revealage : SV_Target1;
}; // struct blend_fs_out

struct shadow_fs_out {
  float depth : SV_Target0;
}; // struct shadow_fs_out

enum material_feature : uint {
  none = 0,
  cast_shadow = (1 << 0),
  receive_shadow = (1 << 1),
  invert_backface_normals = (1 << 2),
}; // struct material_feature

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(0, 1)]]
SamplerState samplers[sbx::common::max_sampler_array_size];

[[vk::binding(0, 2)]]
Texture2D<float4> images[sbx::common::max_image_array_size];

float3x3 build_tbn(gbuffer_fs_in input, material_data material_data) {
  bool invert_backface_normals = (material_data.flags & material_feature::invert_backface_normals) != 0;

  float sign = (invert_backface_normals && !input.sv_is_front_face) ? -1.0 : 1.0;

  float3 N = normalize(input.normal) * sign;
  float3 T = normalize(input.tangent.xyz) * sign;

  // Gram–Schmidt (important for stability)
  T = normalize(T - N * dot(T, N));

  float3 B = cross(N, T) * input.tangent.w;

  return float3x3(T, B, N);
}

float4 get_albedo<AlphaPolicy: alpha_policy>(float2 uv, material_data material_data) {
  uint image_index = material_data.albedo_image_index;
  uint sampler_index = material_data.albedo_sampler_index;

  if (image_index >= sbx::common::max_image_array_size) {
    return material_data.base_color;
  }

  float4 color = images[NonUniformResourceIndex(image_index)].Sample(samplers[NonUniformResourceIndex(sampler_index)], uv);

  return AlphaPolicy::combine_colors(color, material_data.base_color);
}

float3 get_normal(float3x3 TBN, float2 uv, float3 normal, bool is_front_face, material_data material_data) {
  uint image_index = material_data.normal_image_index;
  uint sampler_index = material_data.normal_sampler_index;

  float3 result;

  if (image_index >= sbx::common::max_image_array_size) {
    result = normalize(normal);
  } else {
    float3 sampled_normal = images[NonUniformResourceIndex(image_index)].Sample(samplers[NonUniformResourceIndex(sampler_index)], uv).xyz;
  
    // encode from [0, 1] -> [-1, 1]
    sampled_normal = sampled_normal * 2.0 - 1.0;
  
    sampled_normal.xy *= material_data.normal_scale;
    sampled_normal = normalize(sampled_normal);

    result = normalize(mul(sampled_normal, TBN));
  }

  result *= (is_front_face) ? 1.0 : -1.0;

  return result;
}

float3 get_material(float2 uv, material_data material_data) {
  uint metallic_roughness_image_index = material_data.metallic_roughness_image_index;
  uint metallic_roughness_sampler_index = material_data.metallic_roughness_sampler_index;

  float metallic = material_data.metallic_factor;
  float roughness = material_data.roughness_factor;

  if (metallic_roughness_image_index < sbx::common::max_image_array_size) {
    float4 metallic_roughness = images[NonUniformResourceIndex(metallic_roughness_image_index)].Sample(samplers[NonUniformResourceIndex(metallic_roughness_sampler_index)], uv);

    metallic *= metallic_roughness.b;
    roughness *= metallic_roughness.g;
  }

  uint occlusion_image_index = material_data.occlusion_image_index;
  uint occlusion_sampler_index = material_data.occlusion_sampler_index;

  float occlusion = 1.0;

  if (occlusion_image_index < sbx::common::max_image_array_size) {
    float sampled_occlusion = images[NonUniformResourceIndex(occlusion_image_index)].Sample(samplers[NonUniformResourceIndex(occlusion_sampler_index)], uv).r;

    occlusion = 1.0 + material_data.occlusion_strength * (sampled_occlusion - 1.0);
  }

  return float3(metallic, roughness, occlusion);
}

float3 get_emissive(float2 uv, material_data material_data) {
  uint image_index = material_data.emissive_image_index;
  uint sampler_index = material_data.emissive_sampler_index;

  float3 emissive = material_data.emissive_factor.rgb * material_data.emissive_strength;

  if (image_index >= sbx::common::max_image_array_size) {
    return emissive;
  }

  return emissive * images[NonUniformResourceIndex(image_index)].Sample(samplers[NonUniformResourceIndex(sampler_index)], uv).rgb;
}

float get_height(float2 uv, material_data material_data) {
  uint image_index = material_data.height_image_index;
  uint sampler_index = material_data.height_sampler_index;

  if (image_index >= sbx::common::max_image_array_size) {
    return 0.0f;
  }

  float height = images[NonUniformResourceIndex(image_index)].Sample(samplers[NonUniformResourceIndex(sampler_index)], uv).r;

  return height * material_data.height_scale + material_data.height_offset;
}

float2 parallax_occlusion_mapping(gbuffer_fs_in input, material_data material_data, float3x3 TBN) {
  uint image_index = material_data.height_image_index;

  float2 uv = input.uv * material_data.uv_scale + material_data.uv_offset;

  if (image_index >= sbx::common::max_image_array_size) {
    return uv;
  }

  float3 view_direction_world = normalize(scene.camera_position - input.position);
  float3 view_direction_tangent = normalize(mul(view_direction_world, transpose(TBN)));

  float ndotv = abs(view_direction_tangent.z);

  float num_layers = lerp(material_data.parallax_max_layers, material_data.parallax_min_layers, ndotv);

  num_layers = max(num_layers, 1.0f);

  float layer_depth = 1.0f / num_layers;
  float current_layer_depth = 0.0f;

  float3 v = view_direction_tangent;
  float vz = (abs(v.z) < 1e-4f) ? 1e-4f : v.z;

  float2 delta_uv = v.xy * material_data.height_scale / (vz * num_layers);

  float2 current_uv = uv;
  float height = get_height(current_uv, material_data);

  int step_count = (int)num_layers;

  [loop]
  for (int i = 0; i < step_count; ++i) {
    current_layer_depth += layer_depth;
    current_uv -= delta_uv;

    height = get_height(current_uv, material_data);

    if (height < current_layer_depth) {
      break;
    }
  }

  float2 previous_uv = current_uv + delta_uv;

  float next_depth = height - current_layer_depth;

  float previous_height = get_height(previous_uv, material_data);
  float previous_depth = previous_height - (current_layer_depth - layer_depth);

  float denom = next_depth - previous_depth;

  denom = (abs(denom) < 1e-5f) ? 1e-5f : denom;

  float weight = next_depth / denom;

  return lerp(current_uv, previous_uv, weight);
}

opaque_fs_out write_gbuffer(gbuffer_fs_in input, float2 uv, float3x3 TBN, float4 albedo, instance_data instance_data, material_data material_data) {
  opaque_fs_out output;

  float3 normal = get_normal(TBN, uv, input.normal, input.sv_is_front_face, material_data);
  float3 metallic_roughness_occlusion = get_material(uv, material_data);
  float3 emissive = get_emissive(uv, material_data);

  float material_flags = float(material_data.flags & 255) / 255.0;

  output.albedo = albedo;
  output.position = float4(input.position, 1.0);

  // encode from [-1, 1] -> [0, 1]
  output.normal = float4(normal * 0.5f + 0.5f, 0.0);

  output.material = float4(metallic_roughness_occlusion, material_flags);
  output.emissive = float4(emissive, material_data.specular_factor);
  output.object_id = instance_data.object_id;

  // Linearize depth and encode in [0, 1]
  output.linear_depth = sbx::common::linearize_depth(input.sv_position.z, scene.camera_near, scene.camera_far) / scene.camera_far;

  return output;
}

[noinline] T maybe_unused<T>(T value) {
  return value;
}

[earlydepthstencil]
[shader("fragment")]
opaque_fs_out gbuffer_opaque_main(gbuffer_fs_in input) {
  uint global_instance = input.instance_index;

  instance_data instance_data = push.instance_data_buffer[global_instance];
  material_data material_data = push.material_data_buffer[instance_data.material_index];

  float3x3 TBN = build_tbn(input, material_data);

  // float2 uv = parallax_occlusion_mapping(input, material_data, TBN);
  float2 uv = input.uv * material_data.uv_scale + material_data.uv_offset;

  float4 albedo = get_albedo<opaque_alpha_policy>(uv, material_data);

  return write_gbuffer(input, uv, TBN, albedo, instance_data, material_data);
}

[earlydepthstencil]
[shader("fragment")]
shadow_fs_out shadow_opaque_main(shadow_fs_in input) {
  shadow_fs_out output;

  maybe_unused(images);
  maybe_unused(samplers);
  
  uint global_instance = input.instance_index;

  instance_data instance_data = push.instance_data_buffer[global_instance];
  material_data material_data = push.material_data_buffer[instance_data.material_index];

  output.depth = float(input.sv_position.z);

  return output;
}

[shader("fragment")]
opaque_fs_out gbuffer_mask_main(gbuffer_fs_in input) {
  uint global_instance = input.instance_index;

  instance_data instance_data = push.instance_data_buffer[global_instance];
  material_data material_data = push.material_data_buffer[instance_data.material_index];

  float3x3 TBN = build_tbn(input, material_data);

  // float2 uv = parallax_occlusion_mapping(input, material_data, TBN);
  float2 uv = input.uv * material_data.uv_scale + material_data.uv_offset;

  float4 albedo = get_albedo<mask_alpha_policy>(uv, material_data);

  clip(albedo.a - material_data.alpha_cutoff);

  return write_gbuffer(input, uv, TBN, albedo, instance_data, material_data);
}

[shader("fragment")]
shadow_fs_out shadow_mask_main(shadow_fs_in input) {
  shadow_fs_out output;
  
  uint global_instance = input.instance_index;

  instance_data instance_data = push.instance_data_buffer[global_instance];
  material_data material_data = push.material_data_buffer[instance_data.material_index];

  float2 uv = input.uv * material_data.uv_scale + material_data.uv_offset;

  float4 albedo = get_albedo<mask_alpha_policy>(uv, material_data);

  clip(albedo.a - material_data.alpha_cutoff);

  output.depth = float(input.sv_position.z);

  return output;
}

[shader("fragment")]
blend_fs_out blend_main(gbuffer_fs_in input) {
  blend_fs_out output;

  uint global_instance = input.instance_index;

  instance_data instance_data = push.instance_data_buffer[global_instance];
  material_data material_data = push.material_data_buffer[instance_data.material_index];
  
  float4 albedo = get_albedo<blend_alpha_policy>(input.uv, material_data);

  // Equivalent to: clamp(pow(min(1, a*10)+0.01, 3)*1e8 * pow(1 - z*0.9, 3), 1e-2, 3e3)
  float alpha = clamp(albedo.a, 0.01, 0.99);

  float w_alpha = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
  float w_depth = pow(1.0 - input.sv_position.z * 0.9, 3.0);

  float weight = clamp(w_alpha * 1e8 * w_depth, 1e-2, 3e3);

  output.accumulator = float4(albedo.rgb * albedo.a, albedo.a) * weight;
  output.revealage = albedo.a;

  return output;
}

[shader("fragment")]
void depth_prepass_main(gbuffer_fs_in input) {
  uint global_instance = input.instance_index;

  instance_data instance_data = push.instance_data_buffer[global_instance];
  material_data material_data = push.material_data_buffer[instance_data.material_index];

  float3x3 TBN = build_tbn(input, material_data);

  // float2 uv = parallax_occlusion_mapping(input, material_data, TBN);
  float2 uv = input.uv * material_data.uv_scale + material_data.uv_offset;

  float4 albedo = get_albedo<mask_alpha_policy>(uv, material_data);

  clip(albedo.a - material_data.alpha_cutoff);
}
