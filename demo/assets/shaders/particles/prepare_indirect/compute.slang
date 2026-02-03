#include <particles/common.slang>

[[vk::binding(0, 0)]]
RWStructuredBuffer<counters> counter;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint4> indirect_args;

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

[shader("compute")]
[numthreads(1, 1, 1)]
void main(cs_in input) {
  indirect_args[0] = uint4(6, counter[0].alive_count, 0, 0);
}