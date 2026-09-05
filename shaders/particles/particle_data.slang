// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef PARTICLES_PARTICLE_DATA_SLANG_
#define PARTICLES_PARTICLE_DATA_SLANG_

struct particle {
  float3 position;
  float age;
  float3 velocity;
  float lifetime;
  float size;
  float rotation;
  uint emitter_slot;
  uint seed;
  float4 reserved;
}; // struct particle

enum emission_shape : uint {
  point = 0u,
  sphere = 1u,
  box = 2u
}; // enum emission_shape

static const uint particle_texture_index_none = 0xFFFFFFFFu;

struct emitter_instance {
  float3 position;
  float emission_rate;
  float3 velocity_min;
  float lifetime_min;
  float3 velocity_max;
  float lifetime_max;
  float4 start_color;
  float4 end_color;
  float size_min;
  float size_max;
  float gravity;
  float drag;
  uint active;
  uint particles_to_emit;
  uint seed;
  emission_shape shape;
  float3 shape_extents;
  uint texture_index;
}; // struct emitter_instance

struct particle_counters {
  uint dead_count;
  uint alive_count[2];
  uint pad0;
}; // struct particle_counters

uint pcg_hash(uint input) {
  uint state = input * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

  return (word >> 22u) ^ word;
}

float random_float(inout uint seed) {
  seed = pcg_hash(seed);

  return float(seed) / float(0xFFFFFFFFu);
}

float random_range(inout uint seed, float min, float max) {
  return min + random_float(seed) * (max - min);
}

float3 random_range3(inout uint seed, float3 min, float3 max) {
  return float3(
    random_range(seed, min.x, max.x),
    random_range(seed, min.y, max.y),
    random_range(seed, min.z, max.z)
  );
}

float3 sample_emission_shape(inout uint seed, emission_shape shape, float3 extents) {
  if (shape == emission_shape::sphere) {
    float3 direction = normalize(random_range3(seed, float3(-1.0, -1.0, -1.0), float3(1.0, 1.0, 1.0)) + float3(1e-5, 1e-5, 1e-5));
    float radius = extents.x * pow(random_float(seed), 1.0 / 3.0);

    return direction * radius;
  }

  if (shape == emission_shape::box) {
    return random_range3(seed, -extents, extents);
  }

  return float3(0.0, 0.0, 0.0);
}

#endif // PARTICLES_PARTICLE_DATA_SLANG_
