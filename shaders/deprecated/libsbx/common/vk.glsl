#ifndef COMMON_VK_GLSL_
#define COMMON_VK_GLSL_

struct vk_draw_indirect_command {
  uint vertexCount;
  uint instance_count;
  uint first_vertex;
  uint first_instance;
};

#endif // COMMON_VK_GLSL_
