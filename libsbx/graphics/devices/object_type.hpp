#ifndef LIBSBX_GRAPHICS_DEVICES_OBJECT_TYPE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_OBJECT_TYPE_HPP_

#include <vulkan/vulkan.h>

namespace sbx::graphics {

/**
 * @brief Maps a vulkan handle type to its object type, so debug names can be attached without spelling out the enum at every call site.
 */
template<typename Type>
struct object_type;
 
template<>
struct object_type<VkInstance> {
  inline static constexpr auto value = VK_OBJECT_TYPE_INSTANCE;
}; // struct object_type
 
template<>
struct object_type<VkPhysicalDevice> {
  inline static constexpr auto value = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
}; // struct object_type
 
template<>
struct object_type<VkDevice> {
  inline static constexpr auto value = VK_OBJECT_TYPE_DEVICE;
}; // struct object_type
 
template<>
struct object_type<VkQueue> {
  inline static constexpr auto value = VK_OBJECT_TYPE_QUEUE;
}; // struct object_type
 
template<>
struct object_type<VkSemaphore> {
  inline static constexpr auto value = VK_OBJECT_TYPE_SEMAPHORE;
}; // struct object_type
 
template<>
struct object_type<VkCommandBuffer> {
  inline static constexpr auto value = VK_OBJECT_TYPE_COMMAND_BUFFER;
}; // struct object_type
 
template<>
struct object_type<VkFence> {
  inline static constexpr auto value = VK_OBJECT_TYPE_FENCE;
}; // struct object_type
 
template<>
struct object_type<VkDeviceMemory> {
  inline static constexpr auto value = VK_OBJECT_TYPE_DEVICE_MEMORY;
}; // struct object_type
 
template<>
struct object_type<VkBuffer> {
  inline static constexpr auto value = VK_OBJECT_TYPE_BUFFER;
}; // struct object_type
 
template<>
struct object_type<VkImage> {
  inline static constexpr auto value = VK_OBJECT_TYPE_IMAGE;
}; // struct object_type
 
template<>
struct object_type<VkEvent> {
  inline static constexpr auto value = VK_OBJECT_TYPE_EVENT;
}; // struct object_type
 
template<>
struct object_type<VkQueryPool> {
  inline static constexpr auto value = VK_OBJECT_TYPE_QUERY_POOL;
}; // struct object_type
 
template<>
struct object_type<VkBufferView> {
  inline static constexpr auto value = VK_OBJECT_TYPE_BUFFER_VIEW;
}; // struct object_type
 
template<>
struct object_type<VkImageView> {
  inline static constexpr auto value = VK_OBJECT_TYPE_IMAGE_VIEW;
}; // struct object_type
 
template<>
struct object_type<VkShaderModule> {
  inline static constexpr auto value = VK_OBJECT_TYPE_SHADER_MODULE;
}; // struct object_type
 
template<>
struct object_type<VkPipelineCache> {
  inline static constexpr auto value = VK_OBJECT_TYPE_PIPELINE_CACHE;
}; // struct object_type
 
template<>
struct object_type<VkPipelineLayout> {
  inline static constexpr auto value = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
}; // struct object_type
 
template<>
struct object_type<VkRenderPass> {
  inline static constexpr auto value = VK_OBJECT_TYPE_RENDER_PASS;
}; // struct object_type
 
template<>
struct object_type<VkPipeline> {
  inline static constexpr auto value = VK_OBJECT_TYPE_PIPELINE;
}; // struct object_type
 
template<>
struct object_type<VkDescriptorSetLayout> {
  inline static constexpr auto value = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
}; // struct object_type
 
template<>
struct object_type<VkSampler> {
  inline static constexpr auto value = VK_OBJECT_TYPE_SAMPLER;
}; // struct object_type
 
template<>
struct object_type<VkDescriptorPool> {
  inline static constexpr auto value = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
}; // struct object_type
 
template<>
struct object_type<VkDescriptorSet> {
  inline static constexpr auto value = VK_OBJECT_TYPE_DESCRIPTOR_SET;
}; // struct object_type
 
template<>
struct object_type<VkFramebuffer> {
  inline static constexpr auto value = VK_OBJECT_TYPE_FRAMEBUFFER;
}; // struct object_type
 
template<>
struct object_type<VkCommandPool> {
  inline static constexpr auto value = VK_OBJECT_TYPE_COMMAND_POOL;
}; // struct object_type
 
template<>
struct object_type<VkSamplerYcbcrConversion> {
  inline static constexpr auto value = VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION;
}; // struct object_type
 
template<>
struct object_type<VkDescriptorUpdateTemplate> {
  inline static constexpr auto value = VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE;
}; // struct object_type
 
template<>
struct object_type<VkPrivateDataSlot> {
  inline static constexpr auto value = VK_OBJECT_TYPE_PRIVATE_DATA_SLOT;
}; // struct object_type
 
template<>
struct object_type<VkSurfaceKHR> {
  inline static constexpr auto value = VK_OBJECT_TYPE_SURFACE_KHR;
}; // struct object_type
 
template<>
struct object_type<VkSwapchainKHR> {
  inline static constexpr auto value = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
}; // struct object_type
 
template<typename Type>
constexpr auto object_type_v = object_type<Type>::value;

template<typename Type>
concept named_object_type = requires { 
  object_type<Type>::value; 
}; // concept named_object_type

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_OBJECT_TYPE_HPP_