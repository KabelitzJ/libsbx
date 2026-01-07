#ifndef ALPHA_POLICY_SLANG_
#define ALPHA_POLICY_SLANG_

#include <features.slang>

interface alpha_policy {
  static float4 combine_colors(float4 salmpled, float4 base);
}; // interface alpha_policy

struct opaque_alpha_policy : alpha_policy {
  static float4 combine_colors(float4 salmpled, float4 base) { 
    return float4(salmpled.rgb * base.rgb, 1.0);
  }
}; // struct opaque_alpha_policy

struct mask_alpha_policy : alpha_policy {
  static float4 combine_colors(float4 salmpled, float4 base) { 
    return float4(salmpled.rgb * base.rgb, salmpled.a); 
  }
}; // struct mask_alpha_policy

struct blend_alpha_policy : alpha_policy {
  static float4 combine_colors(float4 salmpled, float4 base) { 
    return float4(salmpled * base); 
  }
}; // struct blend_alpha_policy

#endif // ALPHA_POLICY_SLANG_