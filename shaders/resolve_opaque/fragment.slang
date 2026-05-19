#include <libsbx/common/scene_data.slang>
#include <libsbx/common/depth.slang>
#include <libsbx/common/surface.slang>
#include <libsbx/common/material.slang>
#include <libsbx/common/brdf.slang>
#include <libsbx/common/shadow.slang>
#include <libsbx/common/lighting.slang>
#include <libsbx/common/ibl.slang>

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
  uint directional_light_count;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[[vk::binding(1, 0)]]
StructuredBuffer<sbx::common::point_light> buffer_point_lights;

[[vk::binding(2, 0)]]
StructuredBuffer<sbx::common::directional_light> buffer_directional_lights;

[[vk::binding(3, 0)]]
Sampler2D<float4> albedo_image;

[[vk::binding(4, 0)]]
Sampler2D<float4> position_image;

[[vk::binding(5, 0)]]
Sampler2D<float4> normal_image;

[[vk::binding(6, 0)]]
Sampler2D<float4> material_image;

[[vk::binding(7, 0)]]
Sampler2D<float4> emissive_image;

[[vk::binding(8, 0)]]
Sampler2D<float2> brdf_image;

[[vk::binding(9, 0)]]
SamplerCube<float4> irradiance_image;

[[vk::binding(10, 0)]]
SamplerCube<float4> prefiltered_image;

[[vk::binding(11, 0)]]
Sampler2DArray<float> shadow_image;

sbx::common::surface build_surface(float2 uv) {
  sbx::common::surface surface;

  surface.albedo = albedo_image.Sample(uv).rgb;
  surface.world_position = position_image.Sample(uv).xyz;
  surface.world_normal = normalize(normal_image.Sample(uv).xyz * 2.0 - 1.0);

  float4 material = material_image.Sample(uv);
  surface.metallic = clamp(material.r, 0.0, 1.0);
  surface.roughness = clamp(material.g, 0.045, 1.0);
  surface.ao = clamp(material.b, 0.0, 1.0);
  surface.material_flags = (uint)round(saturate(material.a) * 255.0);

  float4 emissive = emissive_image.Sample(uv);
  surface.emissive = emissive.rgb;
  surface.specular_strength = emissive.a;

  surface.view_direction = normalize(scene.camera_position - surface.world_position);
  surface.ndv = max(dot(surface.world_normal, surface.view_direction), 0.0);
  surface.alpha = surface.roughness * surface.roughness;
  surface.f0 = lerp(sbx::common::default_f0, surface.albedo, surface.metallic);

  return surface;
}

float4 compute_brightness(float3 color) {
  float brightness = dot(color, float3(0.2126, 0.7152, 0.0722));

  return (brightness > 1.0) ? float4(color, 1.0) : float4(0.0, 0.0, 0.0, 1.0);
}

[shader("fragment")]
fs_out main(fs_in input) {
  sbx::common::surface surface = build_surface(input.uv);

  float shadow = sbx::common::calculate_shadow(shadow_image, scene, surface.world_position, surface.world_normal, surface.material_flags);

  float3 sun_direction = normalize(-scene.light_direction);
  float3 sun_radiance = scene.light_color.rgb * scene.light_color.a;

  float3 direct = float3(0.0, 0.0, 0.0);
  direct += sbx::common::evaluate_sun(surface, sun_direction, sun_radiance, shadow);
  direct += sbx::common::evaluate_directional_array(surface, buffer_directional_lights, push.directional_light_count);
  direct += sbx::common::evaluate_point_array(surface, buffer_point_lights, push.point_light_count);

  const float ssao = 1.0;
  float3 ambient = sbx::common::evaluate_ibl(surface, irradiance_image, prefiltered_image, brdf_image, shadow, ssao);

  float3 final_color = ambient + direct + surface.emissive;

  fs_out output;
  output.color = float4(final_color, 1.0);
  output.brightness = compute_brightness(final_color);

  return output;
}
