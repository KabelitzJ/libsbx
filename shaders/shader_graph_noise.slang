// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef SHADER_GRAPH_NOISE_SLANG_
#define SHADER_GRAPH_NOISE_SLANG_

// Shared library functions the shader graph's Simple Noise/Voronoi nodes both compile down to a
// call into (shader_graph_codegen.cpp's _emit) -- only #include'd into a generated file when the
// graph actually has one of those two node types (see noise_helper_source_for), so a graph that
// doesn't use noise gets no unused-function bloat/warnings, and the noise implementation itself
// lives in exactly one place rather than a duplicated text block per generated file.
//
// Ported from Unity Shader Graph's own Simple Noise/Voronoi nodes, with their dead code (an unused
// `uv = abs(frac(uv) - 0.5)` in Value Noise, an unused third res component in Voronoi) dropped and
// their hand-rolled lerp replaced with Slang's own builtin (same as every other node in
// shader_graph_codegen.cpp already uses). Voronoi's Cells output is NOT a true per-cell id -- it
// reads the winning cell's random offset.x, matching a real quirk in Unity's own generated HLSL
// exactly.

float shader_graph_noise_random(float2 uv) {
  return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float shader_graph_noise_value(float2 uv) {
  float2 i = floor(uv);
  float2 f = frac(uv);
  f = f * f * (3.0 - 2.0 * f);

  float r0 = shader_graph_noise_random(i + float2(0.0, 0.0));
  float r1 = shader_graph_noise_random(i + float2(1.0, 0.0));
  float r2 = shader_graph_noise_random(i + float2(0.0, 1.0));
  float r3 = shader_graph_noise_random(i + float2(1.0, 1.0));

  float bottom = lerp(r0, r1, f.x);
  float top = lerp(r2, r3, f.x);
  return lerp(bottom, top, f.y);
}

float shader_graph_simple_noise(float2 uv, float scale) {
  float t = 0.0;

  for (uint octave = 0u; octave < 3u; ++octave) {
    float freq = pow(2.0, float(octave));
    float amp = pow(0.5, float(3u - octave));
    t += shader_graph_noise_value(float2(uv.x * scale / freq, uv.y * scale / freq)) * amp;
  }

  return t;
}

float2 shader_graph_voronoi_random(float2 uv, float angle_offset) {
  float2x2 m = float2x2(15.27, 47.63, 99.41, 89.98);
  uv = frac(sin(mul(uv, m)) * 46839.32);
  return float2(sin(uv.y * angle_offset) * 0.5 + 0.5, cos(uv.x * angle_offset) * 0.5 + 0.5);
}

float2 shader_graph_voronoi(float2 uv, float angle_offset, float cell_density) {
  float2 g = floor(uv * cell_density);
  float2 f = frac(uv * cell_density);

  float2 closest = float2(8.0, 0.0); // x = nearest distance, y = that cell's offset.x ("Cells")

  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      float2 lattice = float2(float(x), float(y));
      float2 offset = shader_graph_voronoi_random(lattice + g, angle_offset);
      float d = distance(lattice + offset, f);

      if (d < closest.x) {
        closest = float2(d, offset.x);
      }
    }
  }

  return closest;
}

#endif // SHADER_GRAPH_NOISE_SLANG_
