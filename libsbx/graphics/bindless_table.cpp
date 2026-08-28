// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/bindless_table.hpp>

#include <algorithm>
#include <array>

#include <libsbx/utility/assert.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/validate.hpp>
#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

auto _are_same_sampler(const sampler::create_info& lhs, const sampler::create_info& rhs) -> bool {
  return lhs.mag_filter == rhs.mag_filter
    && lhs.min_filter == rhs.min_filter
    && lhs.mipmap_mode == rhs.mipmap_mode
    && lhs.address_mode_u == rhs.address_mode_u
    && lhs.address_mode_v == rhs.address_mode_v
    && lhs.address_mode_w == rhs.address_mode_w
    && lhs.max_anisotropy == rhs.max_anisotropy
    && lhs.min_lod == rhs.min_lod
    && lhs.max_lod == rhs.max_lod;
}

auto bindless_table::index_allocator::allocate() -> std::uint32_t {
  if (!released.empty()) {
    const auto index = released.back();
    released.pop_back();

    return index;
  }

  utility::assert_that(next < capacity, "bindless array capacity exceeded");

  return next++;
}

auto bindless_table::index_allocator::release(std::uint32_t index) -> void {
  released.push_back(index);
}

bindless_table::bindless_table(const physical_device& physical_device, const logical_device& logical_device) {
  auto indexing_properties = VkPhysicalDeviceVulkan12Properties{};
  indexing_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;

  auto properties = VkPhysicalDeviceProperties2{};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  properties.pNext = &indexing_properties;

  vkGetPhysicalDeviceProperties2(physical_device, &properties);

  _sampled_images.capacity = std::min(std::uint32_t{16384u}, indexing_properties.maxDescriptorSetUpdateAfterBindSampledImages);
  _samplers.capacity = std::min(std::uint32_t{1024u}, indexing_properties.maxDescriptorSetUpdateAfterBindSamplers);
  _storage_images.capacity = std::min(std::uint32_t{8192u}, indexing_properties.maxDescriptorSetUpdateAfterBindStorageImages);
  _sampled_cubes.capacity = std::min(std::uint32_t{1024u}, indexing_properties.maxDescriptorSetUpdateAfterBindSampledImages);
  _storage_cubes.capacity = std::min(std::uint32_t{512u}, indexing_properties.maxDescriptorSetUpdateAfterBindStorageImages);

  const auto bindings = std::array<VkDescriptorSetLayoutBinding, 5u>{
    VkDescriptorSetLayoutBinding{sampled_image_binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, _sampled_images.capacity, VK_SHADER_STAGE_ALL, nullptr},
    VkDescriptorSetLayoutBinding{sampler_binding, VK_DESCRIPTOR_TYPE_SAMPLER, _samplers.capacity, VK_SHADER_STAGE_ALL, nullptr},
    VkDescriptorSetLayoutBinding{storage_image_binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, _storage_images.capacity, VK_SHADER_STAGE_ALL, nullptr},
    VkDescriptorSetLayoutBinding{sampled_cube_binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, _sampled_cubes.capacity, VK_SHADER_STAGE_ALL, nullptr},
    VkDescriptorSetLayoutBinding{storage_cube_binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, _storage_cubes.capacity, VK_SHADER_STAGE_ALL, nullptr}
  };

  const auto binding_flag = VkDescriptorBindingFlags{VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};
  const auto binding_flags = std::array<VkDescriptorBindingFlags, 5u>{binding_flag, binding_flag, binding_flag, binding_flag, binding_flag};

  auto binding_flags_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{};
  binding_flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  binding_flags_info.bindingCount = static_cast<std::uint32_t>(binding_flags.size());
  binding_flags_info.pBindingFlags = binding_flags.data();

  auto layout_create_info = VkDescriptorSetLayoutCreateInfo{};
  layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_create_info.pNext = &binding_flags_info;
  layout_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  layout_create_info.bindingCount = static_cast<std::uint32_t>(bindings.size());
  layout_create_info.pBindings = bindings.data();

  validate(vkCreateDescriptorSetLayout(logical_device, &layout_create_info, nullptr, &_descriptor_set_layout), "vkCreateDescriptorSetLayout");

  const auto pool_sizes = std::array<VkDescriptorPoolSize, 3u>{
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, _sampled_images.capacity + _sampled_cubes.capacity},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, _samplers.capacity},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, _storage_images.capacity + _storage_cubes.capacity}
  };

  auto pool_create_info = VkDescriptorPoolCreateInfo{};
  pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  pool_create_info.maxSets = 1u;
  pool_create_info.poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size());
  pool_create_info.pPoolSizes = pool_sizes.data();

  validate(vkCreateDescriptorPool(logical_device, &pool_create_info, nullptr, &_descriptor_pool), "vkCreateDescriptorPool");

  auto allocate_info = VkDescriptorSetAllocateInfo{};
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = _descriptor_pool;
  allocate_info.descriptorSetCount = 1u;
  allocate_info.pSetLayouts = &_descriptor_set_layout;

  validate(vkAllocateDescriptorSets(logical_device, &allocate_info, &_descriptor_set), "vkAllocateDescriptorSets");

  auto push_constant_range = VkPushConstantRange{};
  push_constant_range.stageFlags = push_constant_stages;
  push_constant_range.offset = 0u;
  push_constant_range.size = push_constant_size;

  auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{};
  pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_create_info.setLayoutCount = 1u;
  pipeline_layout_create_info.pSetLayouts = &_descriptor_set_layout;
  pipeline_layout_create_info.pushConstantRangeCount = 1u;
  pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;

  validate(vkCreatePipelineLayout(logical_device, &pipeline_layout_create_info, nullptr, &_pipeline_layout), "vkCreatePipelineLayout");

  logical_device.set_debug_name(_descriptor_set_layout, "Bindless Set Layout");
  logical_device.set_debug_name(_descriptor_set, "Bindless Set");
  logical_device.set_debug_name(_pipeline_layout, "Bindless Pipeline Layout");
}

bindless_table::~bindless_table() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  const auto& logical_device = graphics_module.logical_device();

  _sampler_cache.clear();

  vkDestroyPipelineLayout(logical_device, _pipeline_layout, nullptr);
  vkDestroyDescriptorPool(logical_device, _descriptor_pool, nullptr);
  vkDestroyDescriptorSetLayout(logical_device, _descriptor_set_layout, nullptr);
}

auto bindless_table::register_sampled_image(VkImageView view) -> std::uint32_t {
  auto lock = std::lock_guard{_mutex};

  const auto index = _sampled_images.allocate();

  auto image_info = VkDescriptorImageInfo{};
  image_info.imageView = view;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  _pending_writes.push_back(pending_write{sampled_image_binding, index, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, image_info});

  return index;
}

auto bindless_table::reserve_sampled_image() -> std::uint32_t {
  auto lock = std::lock_guard{_mutex};

  return _sampled_images.allocate();
}

auto bindless_table::write_sampled_image(std::uint32_t index, VkImageView view) -> void {
  auto lock = std::lock_guard{_mutex};

  auto image_info = VkDescriptorImageInfo{};
  image_info.imageView = view;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  _pending_writes.push_back(pending_write{sampled_image_binding, index, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, image_info});
}

auto bindless_table::register_storage_image(VkImageView view) -> std::uint32_t {
  auto lock = std::lock_guard{_mutex};

  const auto index = _storage_images.allocate();

  auto image_info = VkDescriptorImageInfo{};
  image_info.imageView = view;
  image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  _pending_writes.push_back(pending_write{storage_image_binding, index, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_info});

  return index;
}

auto bindless_table::unregister_sampled_image(std::uint32_t index) -> void {
  auto lock = std::lock_guard{_mutex};

  _sampled_images.release(index);
}

auto bindless_table::unregister_storage_image(std::uint32_t index) -> void {
  auto lock = std::lock_guard{_mutex};

  _storage_images.release(index);
}

auto bindless_table::register_sampled_cube(VkImageView view) -> std::uint32_t {
  auto lock = std::lock_guard{_mutex};

  const auto index = _sampled_cubes.allocate();

  auto image_info = VkDescriptorImageInfo{};
  image_info.imageView = view;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  _pending_writes.push_back(pending_write{sampled_cube_binding, index, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, image_info});

  return index;
}

auto bindless_table::reserve_sampled_cube() -> std::uint32_t {
  auto lock = std::lock_guard{_mutex};

  return _sampled_cubes.allocate();
}

auto bindless_table::write_sampled_cube(std::uint32_t index, VkImageView view) -> void {
  auto lock = std::lock_guard{_mutex};

  auto image_info = VkDescriptorImageInfo{};
  image_info.imageView = view;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  _pending_writes.push_back(pending_write{sampled_cube_binding, index, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, image_info});
}

auto bindless_table::unregister_sampled_cube(std::uint32_t index) -> void {
  auto lock = std::lock_guard{_mutex};

  _sampled_cubes.release(index);
}

auto bindless_table::register_storage_cube(VkImageView view) -> std::uint32_t {
  auto lock = std::lock_guard{_mutex};

  const auto index = _storage_cubes.allocate();

  auto image_info = VkDescriptorImageInfo{};
  image_info.imageView = view;
  image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  _pending_writes.push_back(pending_write{storage_cube_binding, index, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_info});

  return index;
}

auto bindless_table::unregister_storage_cube(std::uint32_t index) -> void {
  auto lock = std::lock_guard{_mutex};

  _storage_cubes.release(index);
}

auto bindless_table::sampler_index(const sampler::create_info& create_info) -> std::uint32_t {
  auto lock = std::lock_guard{_mutex};

  for (const auto& entry : _sampler_cache) {
    if (_are_same_sampler(entry.create_info, create_info)) {
      return entry.index;
    }
  }

  auto new_sampler = sampler{create_info};

  const auto index = _samplers.allocate();

  auto image_info = VkDescriptorImageInfo{};
  image_info.sampler = new_sampler.handle();

  _pending_writes.push_back(pending_write{sampler_binding, index, VK_DESCRIPTOR_TYPE_SAMPLER, image_info});

  _sampler_cache.push_back(sampler_entry{create_info, std::move(new_sampler), index});

  return index;
}

auto bindless_table::flush_writes() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& logical_device = graphics_module.logical_device();

  // Held across vkUpdateDescriptorSets itself: the set can be flushed from multiple threads and Vulkan requires external sync per VkDescriptorSet.
  auto lock = std::lock_guard{_mutex};

  if (_pending_writes.empty()) {
    return;
  }

  auto writes = std::vector<VkWriteDescriptorSet>{};
  writes.reserve(_pending_writes.size());

  for (auto& entry : _pending_writes) {
    auto write = VkWriteDescriptorSet{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = _descriptor_set;
    write.dstBinding = entry.binding;
    write.dstArrayElement = entry.array_element;
    write.descriptorCount = 1u;
    write.descriptorType = entry.type;
    write.pImageInfo = &entry.image_info;

    writes.push_back(write);
  }

  vkUpdateDescriptorSets(logical_device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0u, nullptr);

  _pending_writes.clear();
}

} // namespace sbx::graphics
