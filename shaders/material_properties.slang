// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef MATERIAL_PROPERTIES_SLANG_
#define MATERIAL_PROPERTIES_SLANG_

#include <frame_data.slang>

// Material properties for a hand-written surface shader. Declare them in one struct named
// `properties` (float, float2, float3, float4, Texture2D and Texture2DArray fields, in any order);
// the engine reflects it, shows the fields in the material inspector, lets C# set them by
// reference (Material.SetFloat/SetColor/SetTexture) and generates load_properties to read them:
//
//   struct properties {
//     [Property("_Strength", "Strength"), Range(0, 1), Default(0.5, 0, 0, 0)] float strength;
//     [Property("_Tint", "Tint"), Color, Hdr, Default(1, 1, 1, 1)] float4 tint;
//     [Property("_Albedo", "Albedo")] Texture2D albedo;
//     [Linear] Texture2D mask; // reference "_mask", shown as "mask"
//   }; // struct properties
//
//   properties p = load_properties(context.material);
//   float4 albedo = p.albedo.Sample(samplers[context.lighting.sampler_index], context.uv) * p.tint;
//
// The values take at most material_max_params * 4 floats, the textures at most
// material_max_textures. Default always takes four numbers; a narrower field uses the first ones.
// A Texture2DArray field can only be set from C#. Including this file requires a `properties` struct.

[__AttributeUsage(_AttributeTargets.Var)]
struct PropertyAttribute {
  string reference;
  string name;
}; // struct PropertyAttribute

// A float4 shown as a color picker.
[__AttributeUsage(_AttributeTargets.Var)]
struct ColorAttribute {
}; // struct ColorAttribute

// A color picker without the 0-1 clamp.
[__AttributeUsage(_AttributeTargets.Var)]
struct HdrAttribute {
}; // struct HdrAttribute

// A texture loaded as unorm instead of sRGB, for data like normal maps or masks.
[__AttributeUsage(_AttributeTargets.Var)]
struct LinearAttribute {
}; // struct LinearAttribute

// A float shown as a slider.
[__AttributeUsage(_AttributeTargets.Var)]
struct RangeAttribute {
  float min;
  float max;
}; // struct RangeAttribute

// The value a material starts with when it's assigned this shader.
[__AttributeUsage(_AttributeTargets.Var)]
struct DefaultAttribute {
  float x;
  float y;
  float z;
  float w;
}; // struct DefaultAttribute

// Only for parsing: the engine compiles the shader with SBX_GENERATED_MATERIAL_LOADER defined and the real load_properties appended.
#ifndef SBX_GENERATED_MATERIAL_LOADER
properties load_properties(material_data material) {
  properties result;
  return result;
}
#endif

#endif // MATERIAL_PROPERTIES_SLANG_
