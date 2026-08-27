// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef SHADING_POLICY_SLANG_
#define SHADING_POLICY_SLANG_

#include <lighting.slang>

struct shading_input {
  lighting_output lighting;
  float depth;
}; // struct shading_input

float4 compute_color(shading_input input) {
  return float4(input.lighting.color, input.lighting.alpha);
}

struct alpha_blend_result {
  float4 accumulator;
  float revealage;
}; // struct alpha_blend_result

alpha_blend_result compute_alpha_blend(shading_input input) {
  float alpha = clamp(input.lighting.alpha, 0.01, 0.99);

  float weight_alpha = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
  float weight_depth = pow(1.0 - input.depth * 0.9, 3.0);
  float weight = clamp(weight_alpha * 1e8 * weight_depth, 1e-2, 3e3);

  alpha_blend_result result;

  result.accumulator = float4(input.lighting.color * alpha, alpha) * weight;
  result.revealage = alpha;

  return result;
}

interface shading_policy {
  associatedtype Output;
  static const bool receives_shadows;
  static Output shade(shading_input input);
}; // interface shading_policy

interface color_target {
  property float4 color { get; set; }
}; // struct color_target

struct basic_color_shading_policy<Target: color_target> : shading_policy {

  typedef Target Output;

  static const bool receives_shadows = true;

  static Output shade(shading_input input) {
    Output output;

    output.color = compute_color(input);

    return output;
  }

}; // struct basic_color_shading_policy

interface alpha_blend_target {
  property float4 accumulator { get; set; }
  property float revealage { get; set; }
}; // struct alpha_blend_target

struct basic_alpha_blend_shading_policy<Target: alpha_blend_target> : shading_policy {

  typedef Target Output;

  static const bool receives_shadows = false;

  static Output shade(shading_input input) {
    Output output;

    alpha_blend_result result = compute_alpha_blend(input);

    output.accumulator = result.accumulator;
    output.revealage = result.revealage;

    return output;
  }

}; // struct basic_alpha_blend_shading_policy

#endif // SHADING_POLICY_SLANG_
