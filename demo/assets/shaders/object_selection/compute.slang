struct cs_in {
  uint3 global_id : SV_DispatchThreadID;
  uint3 local_id : SV_GroupThreadID;
  uint3 group_id : SV_GroupID;
}; // struct cs_in

// static const uint max_selection_count = 516;

// struct selection {
//   uint count;
//   uint ids[max_selection_count];
// }; // struct selection_buffer

// struct push_data {
//   uint2 offset;
//   uint2 size;
//   selection* selection_buffer;
//   uint* visited_objects;
// }; // struct push_data

// [[vk::push_constant]]
// ConstantBuffer<push_data> push;

// [[vk::binding(0, 0)]]
// RWTexture2D<uint> object_id_image;

static const uint local_size_x = 16;
static const uint local_size_y = 16;

[shader("compute")]
[numthreads(local_size_x, local_size_y, 1)]
void main(cs_in input) {
  // uint2 location = input.global_id.xy;

  // if (location.x >= push.size.x || location.y >= push.size.y) {
  //   return;
  // }

  // uint id = object_id_image.Load(location, push.offset);

  // if (id == 0) {
  //   return;
  // }

  // uint word = id >> 5;
  // uint bit = 1u << (id & 31);

  // uint old;
  // InterlockedOr(push.visited_objects[word], bit, old);

  // if ((old & bit) != 0) {
  //   return;
  // }

  // uint index;
  // InterlockedAdd(push.selection_buffer->count, 1, index);

  // if (index >= max_selection_count) {
  //   return;
  // }

  // push.selection_buffer->ids[index] = id;
}
