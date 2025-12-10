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

[[vk::binding(7, 0)]]
Sampler2D<float> ssao_image;

[[vk::binding(8, 0)]]
Sampler2D<float2> brdf_image;

[[vk::binding(9, 0)]]
SamplerCube<float4> irradiance_image;

[[vk::binding(10, 0)]]
SamplerCube<float4> prefiltered_image;

// [[vk::binding(7, 0)]] Sampler2D<uint> object_id_image;
// [[vk::binding(7, 0)]] Sampler2D<float2> shadow_image;

static const float3 DEFAULT_F0 = float3(0.04, 0.04, 0.04);

static const float PREFILTERED_ENV_MAX_MIP = 9.0;

static const float4x4 BIAS_MATRIX = float4x4(
  0.5, 0.0, 0.0, 0.0,
  0.0, 0.5, 0.0, 0.0,
  0.0, 0.0, 0.5, 0.0,
  0.5, 0.5, 0.5, 1.0
);

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

// float calculate_shadow(float3 world_position, float3 normal) {
//   float4 shadow_coord = mul(BIAS_MATRIX, mul(scene.light_space, float4(world_position, 1.0)));
//   float3 projected_coords = shadow_coord.xyz / shadow_coord.w;

//   if (projected_coords.z > 1.0) {
//     return 1.0;
//   }

//   float bias = max(0.0005 * (1.0 - dot(normal, normalize(scene.light_direction))), 0.0001);

//   uint2 dimensions;
//   shadow_image.GetDimensions(dimensions.x, dimensions.y);

//   float2 texel_size = 1.0 / dimensions;

//   float distance_to_camera = length(scene.camera_position - world_position);
//   float radius = lerp(1.0, 4.0, clamp(distance_to_camera / 50.0, 0.0, 1.0));

//   float angle = random2(projected_coords.xy) * sbx::math::two_pi;
//   float2x2 rotation = rotation_matrix(angle);

//   float shadow = 0.0;

//   [unroll]
//   for (int i = 0; i < 16; ++i) {
//     float2 offset = mul(rotation, POISSON_DISK[i]);
//     float2 sample_uv = projected_coords.xy + offset * texel_size * radius;

//     float sample_depth = shadow_image.Sample(sample_uv).r;

//     shadow += (projected_coords.z - bias > sample_depth) ? 0.0 : 1.0;
//   }

//   return shadow / 16.0;
// }

[noinline] T keep<T>(T value) {
  return value;
}

[shader("fragment")]
fs_out main(fs_in in) {
  fs_out out;

  float3 albedo = albedo_image.Sample(in.uv).rgb;
  float3 world_position = position_image.Sample(in.uv).xyz;
  // [0, 1] -> [-1, 1]
  float3 normal = normalize(normal_image.Sample(in.uv).xyz * 2.0f - 1.0f);
  float3 mrao = material_image.Sample(in.uv).rgb;
  float3 emissive = emissive_image.Sample(in.uv).rgb;
  float ssao = ssao_image.Sample(in.uv);

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

  // float shadow = 1.0; 
  // float shadow = calculate_shadow(world_position, normal);

  float3 irradiance = scene.light_color.rgb * ndl;

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

  float3 ambient = (kD_ibl * diffuse_ibl + specular_ibl) * ao * ssao;

  float3 final_color = ambient + color + emissive;

  out.color = float4(final_color, 1.0);

  float brightness = dot(out.color.rgb, float3(0.2126, 0.7152, 0.0722));

  if (brightness > 1.0) {
    out.brightness = float4(out.color.rgb, 1.0);
  } else {
    out.brightness = float4(0.0, 0.0, 0.0, 1.0);
  }

  return out;
}
