#ifndef MATERIAL_ALPHA_POLICY_SLANG_
#define MATERIAL_ALPHA_POLICY_SLANG_

interface alpha_policy {
  static float4 combine_colors(float4 sampled, float4 base);
}; // interface alpha_policy

struct opaque_alpha_policy : alpha_policy {
  static float4 combine_colors(float4 sampled, float4 base) { 
    return float4(sampled.rgb * base.rgb, 1.0);
  }
}; // struct opaque_alpha_policy

struct mask_alpha_policy : alpha_policy {
  static float4 combine_colors(float4 sampled, float4 base) { 
    return float4(sampled.rgb * base.rgb, sampled.a); 
  }
}; // struct mask_alpha_policy

struct blend_alpha_policy : alpha_policy {
  static float4 combine_colors(float4 sampled, float4 base) { 
    return float4(sampled * base); 
  }
}; // struct blend_alpha_policy

#endif // MATERIAL_ALPHA_POLICY_SLANG_