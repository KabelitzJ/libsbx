// #include <libsbx/common/lighting.slang>
// #include <libsbx/common/shadow.slang>
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
Sampler2D<float> shadow0_image;

[[vk::binding(11, 0)]] 
Sampler2D<float> shadow1_image;

[[vk::binding(12, 0)]] 
Sampler2D<float> shadow2_image;

[[vk::binding(13, 0)]] 
Sampler2D<float> shadow3_image;

static const float3 DEFAULT_F0 = float3(0.04, 0.04, 0.04);

static const float PREFILTERED_ENV_MAX_MIP = 9.0;

float3 fresnel_schlick(float cos_theta, float3 f0) {
  return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}

float3 fresnel_schlick_roughness(float cos_theta, float3 F0, float roughness) {
  return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(1.0 - cos_theta, 5.0);
}

float distribution_ggx(float3 n, float3 h, float alpha) {
  float a2 = alpha * alpha;

  float ndh  = max(dot(n, h), 0.0);
  float ndh2 = ndh * ndh;
  
  float denom = ndh2 * (a2 - 1.0) + 1.0;

  return a2 / (sbx::math::pi * denom * denom);
}

float geometry_schlick_ggx(float ndx, float alpha) {
  float k = (alpha + 1.0) * (alpha + 1.0) * 0.125;
  return ndx / (ndx * (1.0 - k) + k);
}

float geometry_smith(float3 n, float3 v, float3 l, float alpha) {
  float ndv = max(dot(n, v), 0.0);
  float ndl = max(dot(n, l), 0.0);

  return geometry_schlick_ggx(ndv, alpha) * geometry_schlick_ggx(ndl, alpha);
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
}; // struct cascade_select

cascade_select select_cascade_with_blend(float view_depth) {
  cascade_select result;
  
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
    result.blend_factor = 0.0;
  }
  
  return result;
}

uint2 shadow_map_dimensions(uint cascade) {
  uint2 dimensions;

  switch (cascade) {
    case 0: {
      shadow0_image.GetDimensions(dimensions.x, dimensions.y);
      break;
    }
    case 1: {
      shadow1_image.GetDimensions(dimensions.x, dimensions.y);
      break;
    }
    case 2: {
      shadow2_image.GetDimensions(dimensions.x, dimensions.y);
      break;
    }
    case 3:
    default: {
      shadow3_image.GetDimensions(dimensions.x, dimensions.y);
      break;
    }
  }

  return dimensions;
}

float sample_shadow_map(uint cascade, float2 uv) {
  switch (cascade) {
    case 0: return shadow0_image.Sample(uv);
    case 1: return shadow1_image.Sample(uv);
    case 2: return shadow2_image.Sample(uv);
    case 3:
    default: return shadow3_image.Sample(uv);
  }
}

float calculate_shadow(uint cascade, float3 world_position, float3 normal) {
  float4 clip = mul(scene.light_spaces[cascade], float4(world_position, 1.0));
  float3 ndc = clip.xyz / clip.w;

  float2 uv = ndc_to_uv(ndc.xy);

  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
    return 1.0;
  }

  float depth = ndc.z;

  if (depth < 0.0 || depth > 1.0) {
    return 1.0;
  }

  float3 light_dir = normalize(-scene.light_direction);
  float bias = max(0.0005 * (1.0 - dot(normal, light_dir)), 0.0001);

  uint2 dimensions = shadow_map_dimensions(cascade);

  float2 texel_size = 1.0 / float2(dimensions);

  float resolution_scale = float(dimensions.x) / 2048.0;
  float base_radius = 1.5 / resolution_scale;
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


[noinline] T maybe_unused<T>(T value) {
  return value;
}

enum material_feature : UINT {
  none = 0,
  emission = (1 << 0),
  normal_map = (1 << 1), 
  occlusion = (1 << 2), 
  height = (1 << 3), 
  clearcoat = (1 << 4), 
  anisotropy = (1 << 5),
  cast_shadow = (1 << 6),
  receive_shadow = (1 << 7)
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
  maybe_unused(shadow0_image);
  maybe_unused(shadow1_image);
  maybe_unused(shadow2_image);
  maybe_unused(shadow3_image);

  float3 albedo = albedo_image.Sample(input.uv).rgb;
  float3 world_position = position_image.Sample(input.uv).xyz;
  // [0, 1] -> [-1, 1]
  float3 normal = normalize(normal_image.Sample(input.uv).xyz * 2.0f - 1.0f);

  float4 material = material_image.Sample(input.uv).rgba;
  float3 mrao = material.rgb;
  uint material_flags = (uint)round(saturate(material.a) * 255.0);

  float3 emissive = emissive_image.Sample(input.uv).rgb;
  // float ssao = ssao_image.Sample(input.uv);

  float metallic = clamp(mrao.r, 0.0, 1.0);
  float roughness = clamp(mrao.g, 0.04, 1.0);
  float ao = clamp(mrao.b, 0.0, 1.0);

  float3 view_dir = normalize(scene.camera_position - world_position);
  float3 f0 = lerp(DEFAULT_F0, albedo, metallic);

  float3 color = float3(0.0, 0.0, 0.0);

  // Global directional light

  float3 light_dir = normalize(-scene.light_direction);
  float3 halfway_dir = normalize(view_dir + light_dir);
  float ndl = max(dot(normal, light_dir), 0.0);

  float alpha = roughness * roughness;

  float D = distribution_ggx(normal, halfway_dir, alpha);
  float G = geometry_smith(normal, view_dir, light_dir, alpha);
  float3 F = fresnel_schlick(max(dot(halfway_dir, view_dir), 0.0), f0);

  float3 numerator = D * G * F;
  float denominator = 4.0 * max(dot(normal, view_dir), 0.0) * ndl + 0.001;
  float3 specular = numerator / denominator;

  float3 k_s = F;
  float3 k_d = (1.0 - k_s) * (1.0 - metallic);

  bool receive_shadow = (material_flags & material_feature::receive_shadow) != 0;

  float shadow = 1.0;

  if (receive_shadow) {
    float view_depth = -mul(scene.view, float4(world_position, 1.0)).z;
    cascade_select cascade_info = select_cascade_with_blend(view_depth);
  
    float shadow_current = calculate_shadow(cascade_info.cascade, world_position, normal);
    
    if (cascade_info.blend_factor > 0.0 && cascade_info.cascade < 3) {
      float shadow_next = calculate_shadow(cascade_info.next_cascade, world_position, normal);
      shadow = lerp(shadow_current, shadow_next, cascade_info.blend_factor);
    } else {
      shadow = shadow_current;
    }
  }

  float3 irradiance = scene.light_color.rgb * ndl * shadow;

  color += (k_d * albedo / sbx::math::pi + specular) * irradiance;

  // Point lights

  for (uint i = 0u; i < push.point_light_count; ++i) {
    point_light light = buffer_point_lights[i];

    float3 light_position = light.position.xyz;
    float light_radius = light.position.w;

    float3 l_dir = light_position - world_position;
    float  distance = max(length(l_dir), 0.001);

    l_dir /= distance;

    float3 h = normalize(view_dir + l_dir);
    float ndl_p = max(dot(normal, l_dir), 0.0);
    float attenuation = clamp(1.0 - distance / light_radius, 0.0, 1.0);

    attenuation *= attenuation;

    float3 radiance = light.color.rgb * light.color.a * attenuation;

    float Dp = distribution_ggx(normal, h, alpha);
    float Gp = geometry_smith(normal, view_dir, l_dir, alpha);
    float3 Fp = fresnel_schlick(max(dot(h, view_dir), 0.0), f0);

    float3 numerator_p = Dp * Gp * Fp;
    float denom_p = 4.0 * max(dot(normal, view_dir), 0.0) * ndl_p + 0.001;
    float3 spec_p = numerator_p / denom_p;

    float3 kd_p = (1.0 - Fp) * (1.0 - metallic);

    color += (kd_p * albedo / sbx::math::pi + spec_p) * radiance * ndl_p;
  }

  // Image based lighting

  float ndv = max(dot(normal, view_dir), 0.0);

  float3 r = normalize(reflect(-view_dir, normal));

  float rough = clamp(roughness, 0.04, 1.0);

  float3 diffuse_irradiance = irradiance_image.Sample(normal).rgb;
  float3 diffuse_ibl = diffuse_irradiance * albedo;

  float lod = rough * PREFILTERED_ENV_MAX_MIP;
  lod = clamp(lod, 0.0, PREFILTERED_ENV_MAX_MIP);

  float3 prefiltered_color = prefiltered_image.SampleLevel(r, lod).rgb;

  float3 F_ibl = fresnel_schlick_roughness(ndv, f0, rough);

  float3 kS_ibl = F_ibl;
  float3 kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);

  float2 brdf = brdf_image.Sample(float2(ndv, rough)).rg;

  float3 specular_ibl = prefiltered_color * (F_ibl * brdf.x + brdf.y);

  // float3 ambient = (kD_ibl * diffuse_ibl + specular_ibl) * ao * ssao;
  float3 ambient = (kD_ibl * diffuse_ibl + specular_ibl) * ao;

  float3 final_color = ambient + color + emissive;

  output.color = float4(final_color, 1.0);

  float brightness = dot(output.color.rgb, float3(0.2126, 0.7152, 0.0722));

  output.brightness = (brightness > 1.0) ? float4(output.color.rgb, 1.0) : float4(0.0, 0.0, 0.0, 1.0);

  return output;
}
