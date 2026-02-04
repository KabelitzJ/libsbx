#ifndef PARTICLES_COMMON_SLANG_
#define PARTICLES_COMMON_SLANG_

static const uint flag_alive = 0x00000001;

static const uint image_index_shift = 1;
static const uint image_index_mask = 0xFFFFFFFE;

struct particle {
  float4 color;
  float3 position;
  float lifetime;
  float3 velocity;
  float age;
  float2 size;
  float rotation;
  uint flags; // bit 0: alive, bits 1-31: texture_index
}; // struct particle

uint get_image_index(uint flags) {
  return flags >> image_index_shift;
}

uint set_image_index(uint flags, uint texture_index) {
  return (flags & flag_alive) | (texture_index << image_index_shift);
}

bool is_alive(uint flags) {
  return (flags & flag_alive) != 0;
}

uint set_alive(uint flags, bool alive) {
  return alive ? (flags | flag_alive) : (flags & ~flag_alive);
}

struct counters {
  uint alive_count;
  uint dead_count;
  uint emit_count;
  uint _pad;
}; // struct counters

struct emitter_params {
  float4 initial_color;
  float4 end_color;
  float3 emitter_position;
  float delta_time;
  float3 emission_shape_min;
  float emission_rate;
  float3 emission_shape_max;
  float time;
  float3 gravity;
  float drag;
  float2 initial_speed;
  float2 initial_lifetime;
  float2 initial_size;
  float2 initial_rotation;
  float end_size_scale;
  uint max_particles;
  uint emit_count;
  uint random_seed;
  uint texture_count;
  uint _pad0;
  uint _pad1;
  uint _pad2;
}; // struct emitter_params

struct render_params {
  float4 end_color; 
  float end_size_scale;
  uint _pad0;
  uint _pad1;
  uint _pad2;
}; // struct render_params


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

float3 random_direction(inout uint seed) {
  float z = random_range(seed, -1.0, 1.0);
  float a = random_range(seed, 0.0, 6.28318);
  float r = sqrt(1.0 - z * z);

  return float3(r * cos(a), r * sin(a), z);
}

#endif // PARTICLES_COMMON_SLANG_