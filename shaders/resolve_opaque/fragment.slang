#include <libsbx/common/scene_data.slang>
#include <libsbx/common/depth.slang>

#include <libsbx/math/constants.slang>

struct fs_in {
  float2 uv : TEXCOORD0;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
  float4 brightness : SV_Target1;
}; // struct fs_out

struct push_data {
  uint point_light_count;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

struct point_light {
  float4 position;
  float4 color;
}; // point_light

[[vk::binding(1, 0)]]
StructuredBuffer<point_light> buffer_point_lights;

[[vk::binding(2, 0)]] 
Sampler2D<float4> albedo_image;

[[vk::binding(3, 0)]] 
Sampler2D<float4> position_image;

[[vk::binding(4, 0)]]
Sampler2D<float4> normal_image;

[[vk::binding(5, 0)]]
Sampler2D<float4> material_image;

[[vk::binding(6, 0)]]
Sampler2D<float4> emissive_image;

// [[vk::binding(7, 0)]]
// Sampler2D<float> ssao_image;

[[vk::binding(7, 0)]]
Sampler2D<float2> brdf_image;
  
[[vk::binding(8, 0)]]
SamplerCube<float4> irradiance_image;

[[vk::binding(9, 0)]]
SamplerCube<float4> prefiltered_image;

[[vk::binding(10, 0)]]
Sampler2DArray<float> shadow_image;

static const float3 DEFAULT_F0 = float3(0.04, 0.04, 0.04);

static const float PREFILTERED_ENV_MAX_MIP = 9.0;

float3 fresnel_schlick(float cos_theta, float3 f0, float specular_factor) {
  return f0 + (specular_factor - f0) * pow(1.0 - cos_theta, 5.0);
}

float3 fresnel_schlick_roughness(float cos_theta, float3 f0, float specular_factor, float roughness) {
  float3 f_max = max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), f0);
  return f0 + (min(f_max, float3(specular_factor, specular_factor, specular_factor)) - f0) * pow(1.0 - cos_theta, 5.0);
}

float distribution_ggx(float3 n, float3 h, float alpha) {
  float a2 = alpha * alpha;

  float ndh  = max(dot(n, h), 0.0);
  float ndh2 = ndh * ndh;
  
  float denom = ndh2 * (a2 - 1.0) + 1.0;

  return a2 / (sbx::math::pi * denom * denom);
}

float V_smith_ggx_correlated(float ndv, float ndl, float alpha) {
  float a2 = alpha * alpha;
  float ggx_v = ndl * sqrt(ndv * ndv * (1.0 - a2) + a2);
  float ggx_l = ndv * sqrt(ndl * ndl * (1.0 - a2) + a2);

  return 0.5 / max(ggx_v + ggx_l, 1e-5);
}

static const float2 POISSON_DISK[16] = {
  float2(-0.94201624, -0.39906216),
  float2( 0.94558609, -0.76890725),
  float2(-0.094184101, -0.92938870),
  float2( 0.34495938,  0.29387760),
  float2(-0.91588581,  0.45771432),
  float2(-0.81544232, -0.87912464),
  float2(-0.38277543,  0.27676845),
  float2( 0.97484398,  0.75648379),
  float2( 0.44323325, -0.97511554),
  float2( 0.53742981, -0.47373420),
  float2(-0.26496911, -0.41893023),
  float2( 0.79197514,  0.19090188),
  float2(-0.24188840,  0.99706507),
  float2(-0.81409955,  0.91437590),
  float2( 0.19984126,  0.78641367),
  float2( 0.14383161, -0.14100790)
};

float random2(float2 seed) {
  return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}

float2x2 rotation_matrix(float angle) {
  float s = sin(angle);
  float c = cos(angle);

  return float2x2(c, -s, s, c);
}

static const float2x2 NDC_TO_UV_SCALE = float2x2(
  0.5, 0.0,
  0.0, 0.5
);

static const float2 NDC_TO_UV_BIAS = float2(0.5, 0.5);

float2 ndc_to_uv(float2 ndc_xy) {
  return mul(NDC_TO_UV_SCALE, ndc_xy) + NDC_TO_UV_BIAS;
}

struct cascade_select {
  uint cascade;
  float blend_factor;
  uint next_cascade;
  bool fade_out;
}; // struct cascade_select

cascade_select select_cascade_with_blend(float view_depth) {
  cascade_select result;
  result.fade_out = false;
  
  const float blend_threshold = 0.9;
  
  if (view_depth < scene.cascade_splits.x) {
    result.cascade = 0;
    result.next_cascade = 1;
    float blend_start = scene.cascade_splits.x * blend_threshold;
    result.blend_factor = saturate((view_depth - blend_start) / (scene.cascade_splits.x - blend_start));
  } else if (view_depth < scene.cascade_splits.y) {
    result.cascade = 1;
    result.next_cascade = 2;
    float blend_start = scene.cascade_splits.y * blend_threshold;
    result.blend_factor = saturate((view_depth - blend_start) / (scene.cascade_splits.y - blend_start));
  } else if (view_depth < scene.cascade_splits.z) {
    result.cascade = 2;
    result.next_cascade = 3;
    float blend_start = scene.cascade_splits.z * blend_threshold;
    result.blend_factor = saturate((view_depth - blend_start) / (scene.cascade_splits.z - blend_start));
  } else {
    result.cascade = 3;
    result.next_cascade = 3;
    result.fade_out = true;
    float cascade3_far = scene.cascade_splits.w;
    float fade_start = cascade3_far * blend_threshold;
    result.blend_factor = saturate((view_depth - fade_start) / (cascade3_far - fade_start));
  }
  
  return result;
}

uint2 shadow_map_dimensions(uint cascade) {
  uint2 dimensions;
  uint elements;
  shadow_image.GetDimensions(dimensions.x, dimensions.y, elements);
  return dimensions;
}

float sample_shadow_map(uint cascade, float2 uv) {
  return shadow_image.Sample(float3(uv, float(cascade)));
}

float calculate_shadow_cascade(uint cascade, float3 world_position, float3 normal) {
  uint2 dimensions = shadow_map_dimensions(cascade);
  float2 texel_size = 1.0 / float2(dimensions);

  float3 light_direction = normalize(-scene.light_direction);
  float ndl = dot(normal, light_direction);

  // Normal offset bias — push sample point along normal proportional to texel size
  float normal_offset_scale = texel_size.x * 2.0 * (1.0 + cascade);
  float3 offset_position = world_position + normal * normal_offset_scale * saturate(1.0 - ndl);

  float4 clip = mul(scene.light_spaces[cascade], float4(offset_position, 1.0));
  float3 ndc = clip.xyz / clip.w;

  float2 uv = ndc_to_uv(ndc.xy);

  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
    return 1.0;
  }

  float depth = ndc.z;

  if (depth < 0.0 || depth > 1.0) {
    return 1.0;
  }

  float base_bias = max(0.002 * (1.0 - ndl), 0.0005);
  float bias = base_bias * (1.0 + cascade * 0.5);

  float resolution_scale = float(dimensions.x) / 2048.0;
  float base_radius = 1.0 / resolution_scale;
  float radius = base_radius * (1.0 + cascade * 0.3);

  float angle = random2(uv) * sbx::math::two_pi;
  float2x2 rotation = rotation_matrix(angle);

  float shadow = 0.0;

  [unroll]
  for (int i = 0; i < 16; ++i) {
    float2 offset = mul(rotation, POISSON_DISK[i]);
    float2 sample_uv = uv + offset * texel_size * radius;

    float sample_depth = sample_shadow_map(cascade, sample_uv);

    shadow += (depth - bias <= sample_depth) ? 1.0 : 0.0;
  }

  return shadow / 16.0;
}

float calculate_shadow(float3 world_position, float3 normal, uint material_flags) {
  float shadow = 1.0;

  bool receive_shadow = (material_flags & material_feature::receive_shadow) != 0;

  if (receive_shadow) {
    float view_depth = -mul(scene.view, float4(world_position, 1.0)).z;

    cascade_select cascade_info = select_cascade_with_blend(view_depth);
  
    float shadow_current = calculate_shadow_cascade(cascade_info.cascade, world_position, normal);
    
    if (cascade_info.fade_out) {
      shadow = lerp(shadow_current, 1.0, cascade_info.blend_factor);
    } else if (cascade_info.blend_factor > 0.0 && cascade_info.cascade < 3) {
      float shadow_next = calculate_shadow_cascade(cascade_info.next_cascade, world_position, normal);

      shadow = lerp(shadow_current, shadow_next, cascade_info.blend_factor);
    } else {
      shadow = shadow_current;
    }
  }

  return shadow;
}


[noinline] T maybe_unused<T>(T value) {
  return value;
}

enum material_feature : uint {
  none = 0,
  cast_shadow = (1 << 1),
  receive_shadow = (1 << 2),
  invert_backface_normals = (1 << 3),
}; // struct material_feature

[shader("fragment")]
fs_out main(fs_in input) {
  fs_out output;

  maybe_unused(position_image);
  maybe_unused(normal_image);
  maybe_unused(albedo_image);
  maybe_unused(material_image);
  maybe_unused(emissive_image);
  maybe_unused(irradiance_image);
  maybe_unused(brdf_image);
  maybe_unused(prefiltered_image);
  maybe_unused(shadow_image);

  float3 albedo = albedo_image.Sample(input.uv).rgb;
  float3 world_position = position_image.Sample(input.uv).xyz;
  // [0, 1] -> [-1, 1]
  float3 normal = normalize(normal_image.Sample(input.uv).xyz * 2.0f - 1.0f);

  float4 material = material_image.Sample(input.uv).rgba;
  float3 mrao = material.rgb;
  uint material_flags = (uint)round(saturate(material.a) * 255.0);

  float4 emissive = emissive_image.Sample(input.uv);
  float3 emissive_color = emissive.rgb;
  float specular_strength = emissive.a;

  // float ssao = ssao_image.Sample(input.uv);
  float ssao = 1.0;

  float metallic = clamp(mrao.r, 0.0, 1.0);
  float roughness = clamp(mrao.g, 0.045, 1.0);
  float ao = clamp(mrao.b, 0.0, 1.0);

  float3 view_direction = normalize(scene.camera_position - world_position);
  float ndv = max(dot(normal, view_direction), 0.0);
  float3 f0 = lerp(DEFAULT_F0, albedo, metallic);

  float3 color = float3(0.0, 0.0, 0.0);

  // Global directional light

  float3 light_direction = normalize(-scene.light_direction);
  float3 halfway_direction = normalize(view_direction + light_direction);
  float ndl = max(dot(normal, light_direction), 0.0);

  float alpha = roughness * roughness;

  float D = distribution_ggx(normal, halfway_direction, alpha);
  float V = V_smith_ggx_correlated(ndv, ndl, alpha);
  float3 F = fresnel_schlick(max(dot(halfway_direction, view_direction), 0.0), f0, specular_strength);

  float3 specular = D * V * F;

  float3 k_s = F;
  float3 k_d = (1.0 - k_s) * (1.0 - metallic);

  float shadow = calculate_shadow(world_position, normal, material_flags);

  float3 irradiance = scene.light_color.rgb * ndl * shadow;

  color += (k_d * albedo / sbx::math::pi + specular) * irradiance;

  // Point lights

  for (uint i = 0u; i < push.point_light_count; ++i) {
    point_light light = buffer_point_lights[i];

    float3 light_position = light.position.xyz;
    float light_radius = light.position.w;

    float3 light_direction = light_position - world_position;
    float  distance = max(length(light_direction), 0.001);

    light_direction /= distance;

    float3 h = normalize(view_direction + light_direction);
    float ndl_p = max(dot(normal, light_direction), 0.0);

    float d_over_r = distance / light_radius;
    float d_over_r_4 = d_over_r * d_over_r * d_over_r * d_over_r;
    float clamped = saturate(1.0 - d_over_r_4);
    float attenuation = (clamped * clamped) / max(distance * distance, 1e-4);

    float3 radiance = light.color.rgb * light.color.a * attenuation;

    float Dp = distribution_ggx(normal, h, alpha);
    float Vp = V_smith_ggx_correlated(ndv, ndl_p, alpha);
    float3 Fp = fresnel_schlick(max(dot(h, view_direction), 0.0), f0, specular_strength);

    float3 spec_p = Dp * Vp * Fp;

    float3 kd_p = (1.0 - Fp) * (1.0 - metallic);

    color += (kd_p * albedo / sbx::math::pi + spec_p) * radiance * ndl_p;
  }

  // Image based lighting

  float3 r = normalize(reflect(-view_direction, normal));

  float3 diffuse_irradiance = irradiance_image.Sample(normal).rgb;
  float3 diffuse_ibl = diffuse_irradiance * albedo / sbx::math::pi;

  float lod = roughness * PREFILTERED_ENV_MAX_MIP;
  lod = clamp(lod, 0.0, PREFILTERED_ENV_MAX_MIP);

  float3 prefiltered_color = prefiltered_image.SampleLevel(r, lod).rgb;

  float3 F_ibl = fresnel_schlick_roughness(ndv, f0, specular_strength, roughness);

  float3 kS_ibl = F_ibl;
  float3 kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);

  float2 brdf = brdf_image.Sample(float2(ndv, roughness)).rg;

  float3 specular_ibl = prefiltered_color * (F_ibl * brdf.x + brdf.y);

  float shadow_occlusion = lerp(0.3, 1.0, shadow);
  float3 ambient = (kD_ibl * diffuse_ibl * shadow_occlusion + specular_ibl) * ao * ssao;

  float3 final_color = ambient + color + emissive_color;

  output.color = float4(final_color, 1.0);

  float brightness = dot(output.color.rgb, float3(0.2126, 0.7152, 0.0722));

  output.brightness = (brightness > 1.0) ? float4(output.color.rgb, 1.0) : float4(0.0, 0.0, 0.0, 1.0);

  return output;
}
