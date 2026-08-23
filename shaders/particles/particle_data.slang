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
  uint pad0;
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

float random_range(inout uint seed, float min_val, float max_val) {
  return min_val + random_float(seed) * (max_val - min_val);
}

float3 random_range3(inout uint seed, float3 min_val, float3 max_val) {
  return float3(
    random_range(seed, min_val.x, max_val.x),
    random_range(seed, min_val.y, max_val.y),
    random_range(seed, min_val.z, max_val.z)
  );
}

#endif // PARTICLES_PARTICLE_DATA_SLANG_
