// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

struct vs_in {
  [[vk::location(0)]] uint vertex_id : SV_VertexID;
  [[vk::location(1)]] uint instance_id : SV_InstanceID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 uv : TEXCOORD;
  [[vk::location(3)]] float4 tangent : TANGENT;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct fs_in {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 uv : TEXCOORD;
  [[vk::location(3)]] float4 tangent : TANGENT;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

struct vertex {
  float3 position;
  float3 normal;
  float2 uv;
  float4 tangent;
}; // struct vertex

struct material_data {
  float4 base_color_factor;
  float4 emissive_factor;
  uint albedo_index;
  uint normal_index;
  uint metallic_roughness_index;
  uint occlusion_index;
  uint emissive_index;
  float metallic_factor;
  float roughness_factor;
  float alpha_cutoff;
  uint flags;
  uint pad0;
  uint pad1;
  uint pad2;
}; // struct material_data

enum class light_type : uint {
  directional = 0,
  point = 1,
  spot = 2
}; // enum light_type

struct light_data {
  float4 color;
  float4 position;
  float4 direction;
  light_type type;
  float inner_cos;
  float outer_cos;
  uint padding;
}; // struct light_data

struct frame_data {
  float4x4 view;
  float4x4 projection;
  float4 camera_position;
  light_data* lights;
  uint light_count;
  uint padding;
  material_data* materials;
}; // struct frame_data

struct push_data {
  frame_data* frame;
  vertex* vertices;
  float4x4* transforms;
  uint transform_offset;
  uint material_index;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]] Texture2D textures[];
[[vk::binding(1, 0)]] SamplerState samplers[];

static const float pi = 3.14159265359;

float distribution_ggx(float3 n, float3 h, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float ndh = max(dot(n, h), 0.0);
  float d = ndh * ndh * (a2 - 1.0) + 1.0;
  return a2 / (pi * d * d);
}

float geometry_schlick_ggx(float ndv, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return ndv / (ndv * (1.0 - k) + k);
}

float geometry_smith(float3 n, float3 v, float3 l, float roughness) {
  return geometry_schlick_ggx(max(dot(n, v), 0.0), roughness) * geometry_schlick_ggx(max(dot(n, l), 0.0), roughness);
}

float3 fresnel_schlick(float cos_theta, float3 f0) {
  return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float3 brdf(float3 n, float3 v, float3 l, float3 albedo, float3 f0, float roughness, float metallic) {
  float3 h = normalize(v + l);
  float ndf = distribution_ggx(n, h, roughness);
  float g = geometry_smith(n, v, l, roughness);
  float3 f = fresnel_schlick(max(dot(h, v), 0.0), f0);

  float3 specular = (ndf * g * f) / max(4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0), 0.0001);
  float3 kd = (1.0 - f) * (1.0 - metallic);

  return (kd * albedo / pi + specular) * max(dot(n, l), 0.0);
}

[shader("vertex")]
vs_out vertex_main(vs_in input) {
  vertex v = push.vertices[input.vertex_id];
  frame_data frame = *push.frame;

  float4x4 model = push.transforms[push.transform_offset + input.instance_id];

  float4 world = mul(model, float4(v.position, 1.0));
  float3x3 normal_matrix = (float3x3)model;

  vs_out output;
  output.position = world.xyz;
  output.normal = normalize(mul(normal_matrix, v.normal));
  output.tangent = float4(normalize(mul(normal_matrix, v.tangent.xyz)), v.tangent.w);
  output.uv = v.uv;
  output.sv_position = mul(frame.projection, mul(frame.view, world));

  return output;
}

[shader("fragment")]
fs_out fragment_main(fs_in input) {
  frame_data frame = *push.frame;
  material_data material = frame.materials[push.material_index];
  SamplerState sampler = samplers[push.sampler_index];

  float4 base = material.base_color_factor * textures[material.albedo_index].Sample(sampler, input.uv);

  if ((material.flags & 1u) != 0u && base.a < material.alpha_cutoff) {
    discard;
  }

  float3 albedo = base.rgb;

  float4 mr = textures[material.metallic_roughness_index].Sample(sampler, input.uv);
  float metallic = mr.b * material.metallic_factor;
  float roughness = mr.g * material.roughness_factor;

  float ao = textures[material.occlusion_index].Sample(sampler, input.uv).r;
  float3 emissive = material.emissive_factor.rgb * textures[material.emissive_index].Sample(sampler, input.uv).rgb;

  float3 n = normalize(input.normal);
  float3 t = normalize(input.tangent.xyz);

  // Gram-Schmidt re-orthogonalize after interpolation
  t = normalize(t - dot(t, n) * n);              

  // handedness
  float3 b = cross(n, t) * input.tangent.w; 

  float3 tangent_normal = textures[material.normal_index].Sample(sampler, input.uv).rgb * 2.0 - 1.0;
  n = normalize(tangent_normal.x * t + tangent_normal.y * b + tangent_normal.z * n);

  float3 v = normalize(frame.camera_position.xyz - input.position);

  float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
  float3 lo = float3(0.0, 0.0, 0.0);

  for (uint i = 0u; i < frame.light_count; ++i) {
    light_data light = frame.lights[i];

    float3 l;
    float attenuation = 1.0;

    if (light.type == light_type::directional) {
      l = normalize(-light.direction.xyz);
    } else {
      float3 to_light = light.position.xyz - input.position;
      float distance = length(to_light);
      l = to_light / max(distance, 0.0001);

      float range = light.position.w;
      float falloff = saturate(1.0 - pow(distance / max(range, 0.0001), 4.0));
      attenuation = (falloff * falloff) / max(distance * distance, 0.0001);

      if (light.type == light_type::spot) {
        float cos_angle = dot(-l, normalize(light.direction.xyz));
        attenuation *= saturate((cos_angle - light.outer_cos) / max(light.inner_cos - light.outer_cos, 0.0001));
      }
    }

    float3 radiance = light.color.rgb * light.color.a * attenuation;
    lo += brdf(n, v, l, albedo, f0, roughness, metallic) * radiance;
  }

  float3 ambient = 0.03 * albedo * ao;
  float3 color = ambient + lo + emissive;

  fs_out output;
  output.color = float4(color, base.a);

  return output;
}
