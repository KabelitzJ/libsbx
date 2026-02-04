#include <particles/common.slang>

[[vk::binding(0, 0)]]
RWStructuredBuffer<particle> particles;

[[vk::push_constant]]
ConstantBuffer<emitter_params> params;

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

[shader("compute")]
[numthreads(256, 1, 1)]
void main(cs_in input) {
  uint index = input.id.x;

  if (index >= params.emit_count) {
    return;
  }

  uint seed = params.random_seed + index * 7919u;
  uint start = pcg_hash(seed) % params.max_particles;
  uint slot = start;

  for (uint i = 0; i < params.max_particles; ++i) {
    slot = (start + i) % params.max_particles;

    if ((particles[slot].flags & flag_alive) == 0) {
      break;
    }
  }

  if ((particles[slot].flags & flag_alive) != 0) {
    return;
  }

  particle particle;

  particle.position = params.emitter_position + random_range3(seed, params.emission_shape_min, params.emission_shape_max);

  float speed = random_range(seed, params.initial_speed.x, params.initial_speed.y);
  float3 direction = random_direction(seed);
  direction.y = abs(direction.y);
  particle.velocity = direction * speed;

  particle.lifetime = random_range(seed, params.initial_lifetime.x, params.initial_lifetime.y);
  particle.age = 0.0;
  particle.color = params.initial_color;
  particle.size = float2(random_range(seed, params.initial_size.x, params.initial_size.y));
  particle.rotation = random_range(seed, params.initial_rotation.x, params.initial_rotation.y);
  particle.flags = flag_alive;

  particles[slot] = particle;
}
