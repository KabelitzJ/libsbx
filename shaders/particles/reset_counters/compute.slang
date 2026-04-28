#include <particles/common.slang>

[[vk::binding(0, 0)]]
RWStructuredBuffer<counters> counter;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
  counter[0].alive_count = 0;
  counter[0].dead_count = 0;
}