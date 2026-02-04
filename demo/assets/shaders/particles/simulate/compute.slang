#include <particles/common.slang>

[[vk::binding(0, 0)]]
RWStructuredBuffer<particle> particles;

[[vk::binding(1, 0)]]
RWStructuredBuffer<counters> counter;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> alive_list;

[[vk::push_constant]]
ConstantBuffer<emitter_params> params;

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

[shader("compute")]
[numthreads(256, 1, 1)]
void main(cs_in input) {
  uint index = input.id.x;

  if (index >= params.max_particles) {
    return;
  }

  particle particle = particles[index];

  if (!is_alive(particle.flags)) {
    return;
  }

  particle.age += params.delta_time;

  if (particle.age >= particle.lifetime) {
    particle.flags = set_alive(particle.flags, false);
    particles[index] = particle;
    InterlockedAdd(counter[0].dead_count, 1);

    return;
  }

  particle.velocity += params.gravity * params.delta_time;
  particle.velocity *= 1.0 - params.drag * params.delta_time;
  particle.position += particle.velocity * params.delta_time;

  particles[index] = particle;

  uint alive_index;
  InterlockedAdd(counter[0].alive_count, 1, alive_index);

  alive_list[alive_index] = index;
}
