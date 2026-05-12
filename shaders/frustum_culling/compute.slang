struct draw_command {
  uint index_count;
  uint instance_count;
  uint first_index;
  int vertex_offset;
  uint first_instance;
}; // struct draw_command

struct instance_data {
  uint transform_index;
  uint material_index;
  uint object_id;
  uint payload;
}; // struct instance_data

struct transform_data {
  float4x4 model;
  float4x4 normal;
}; // struct transform_data

struct local_aabb {
  float4 center;
  float4 extents;
}; // struct local_aabb

struct frustum_planes {
  float4 planes[6];
}; // struct frustum_planes

struct push_data {
  draw_command* input_commands;
  instance_data* input_instances;
  draw_command* output_commands;
  instance_data* output_instances;
  transform_data* transforms;
  local_aabb* bounds;
  uint* prefix_sum;
  frustum_planes* frustum;
  uint command_count;
  uint instance_count;
  float radius_scale;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

uint find_command_index(uint instance_id) {
  uint low = 0;
  uint high = push.command_count;

  while (low < high) {
    uint mid = (low + high) / 2;

    if (push.prefix_sum[mid] <= instance_id) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }

  return low - 1;
}

bool test_sphere_frustum(float3 center, float radius) {
  frustum_planes f = push.frustum[0];

  for (int i = 0; i < 6; i++) {
    float d = dot(f.planes[i].xyz, center) + f.planes[i].w;

    if (d < -radius) {
      return false;
    }
  }

  return true;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
  uint instance_id = dispatch_thread_id.x;

  if (instance_id >= push.instance_count) {
    return;
  }

  uint command_index = find_command_index(instance_id);

  draw_command input_command = push.input_commands[command_index];
  instance_data instance_data = push.input_instances[instance_id];
  local_aabb aabb = push.bounds[command_index];

  float4x4 world = push.transforms[instance_data.transform_index].model;

  float3 world_center = mul(world, float4(aabb.center.xyz, 1.0)).xyz;

  float scale_x = length(world[0].xyz);
  float scale_y = length(world[1].xyz);
  float scale_z = length(world[2].xyz);
  float max_scale = max(scale_x, max(scale_y, scale_z));
  float radius = length(aabb.extents.xyz) * max_scale * push.radius_scale;

  if (test_sphere_frustum(world_center, radius)) {
    uint slot;
    InterlockedAdd(push.output_commands[command_index].instance_count, 1, slot);

    if (slot == 0) {
      push.output_commands[command_index].index_count = input_command.index_count;
      push.output_commands[command_index].first_index = input_command.first_index;
      push.output_commands[command_index].vertex_offset = input_command.vertex_offset;
      push.output_commands[command_index].first_instance = input_command.first_instance;
    }

    push.output_instances[input_command.first_instance + slot] = instance_data;
  }
}
