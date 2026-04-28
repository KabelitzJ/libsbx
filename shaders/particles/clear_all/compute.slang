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

  if (index >= params.max_particles) {
    return;
  }

  particles[index].flags = 0;
}
