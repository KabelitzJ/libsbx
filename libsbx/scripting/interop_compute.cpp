// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/interop.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <vulkan/vulkan.h>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/bindless_table.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/sampler.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/commands/fence.hpp>
#include <libsbx/graphics/pipeline/shader_cache.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline_cache.hpp>

#include <libsbx/scripting/scripting_module.hpp>

namespace sbx::scripting {

using push_constant_field = graphics::shader_compiler::push_constant_field;

auto kind_name(push_constant_field::kind kind) -> std::string_view {
  switch (kind) {
    case push_constant_field::kind::f32: return "float";
    case push_constant_field::kind::i32: return "int";
    case push_constant_field::kind::u32: return "uint";
    case push_constant_field::kind::f32x2: return "float2";
    case push_constant_field::kind::f32x3: return "float3";
    case push_constant_field::kind::f32x4: return "float4";
    case push_constant_field::kind::sampled_texture: return "sampled_texture";
    case push_constant_field::kind::storage_texture: return "storage_texture";
    case push_constant_field::kind::sampler: return "sampler_handle";
    case push_constant_field::kind::buffer: return "buffer pointer";
  }

  return "?";
}

auto find_buffer(std::uint64_t id, std::string_view caller) -> compute_buffer_state* {
  auto& registry = core::engine::get_module<scripting_module>().resources().compute_buffers;

  if (const auto entry = registry.find(id); entry != registry.end()) {
    return &entry->second;
  }

  utility::logger<"scripting">::error("{}: unknown compute buffer {}", caller, id);

  return nullptr;
}

auto find_shader(std::uint64_t id, std::string_view caller) -> compute_shader_state* {
  auto& registry = core::engine::get_module<scripting_module>().resources().compute_shaders;

  if (const auto entry = registry.find(id); entry != registry.end()) {
    return &entry->second;
  }

  utility::logger<"scripting">::error("{}: unknown compute shader {}", caller, id);

  return nullptr;
}

// Logs and returns nullopt on a missing field or a kind mismatch.
auto find_field(compute_shader_state& state, const std::string& name, push_constant_field::kind expected) -> std::optional<std::size_t> {
  const auto& fields = state.layout.fields;

  const auto field = std::ranges::find(fields, name, &push_constant_field::name);

  if (field == fields.end()) {
    auto known = std::string{};

    for (const auto& entry : fields) {
      known += known.empty() ? entry.name : ", " + entry.name;
    }

    utility::logger<"scripting">::error("ComputeShader '{}': no push_data field named '{}' (fields: {})", state.path.filename().string(), name, known);

    return std::nullopt;
  }

  if (field->type != expected) {
    utility::logger<"scripting">::error("ComputeShader '{}': field '{}' is a {}, but was set as a {}", state.path.filename().string(), name, kind_name(field->type), kind_name(expected));

    return std::nullopt;
  }

  return static_cast<std::size_t>(std::distance(fields.begin(), field));
}

auto write_field(compute_shader_state& state, std::size_t index, const void* value) -> void {
  const auto& field = state.layout.fields[index];

  std::memcpy(state.params.data() + field.offset, value, field.size);

  state.is_set[index] = true;
}


auto interop::compute_buffer_create(std::int32_t count, std::int32_t stride, std::uint32_t access) -> std::uint64_t {
  if (count < 0 || stride <= 0) {
    utility::logger<"scripting">::error("compute_buffer_create: count must be >= 0 and stride > 0 (count={}, stride={})", count, stride);
    return 0u;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  // At least one element, so an empty buffer still has a valid device address to bind.
  const auto size = static_cast<graphics::buffer::size_type>(std::max(count, 1)) * static_cast<graphics::buffer::size_type>(stride);

  // Host-visible so writes go straight through the mapping; upload_context's deferred flush would land after a same-call bake.
  // ponytail: GPU reads/writes host memory over the bus; add a device_local copy if a bake gets bandwidth-bound.
  const auto handle = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = size,
    .usage = graphics::buffer_usage::storage | graphics::buffer_usage::device_address,
    .memory = access == 1u ? graphics::memory_usage::host_read : graphics::memory_usage::host_write,
    .name = "Script Compute Buffer"
  });

  const auto id = math::uuid::create().value();
  core::engine::get_module<scripting_module>().resources().compute_buffers.emplace(id, compute_buffer_state{handle, static_cast<std::uint32_t>(stride)});

  return id;
}

auto interop::compute_buffer_set_data(std::uint64_t id, const void* data, std::int32_t byte_count) -> managed::bool32 {
  auto* state = find_buffer(id, "compute_buffer_set_data");

  if (!state) {
    return false;
  }

  if (!data || byte_count <= 0) {
    return true;
  }

  auto& buffer = core::engine::get_module<graphics::graphics_module>().resource_registry().get<graphics::buffer>(state->handle);

  if (static_cast<graphics::buffer::size_type>(byte_count) > buffer.size()) {
    utility::logger<"scripting">::error("compute_buffer_set_data: {} bytes exceed buffer {}'s size {}", byte_count, id, buffer.size());
    return false;
  }

  buffer.write(data, static_cast<graphics::buffer::size_type>(byte_count));

  return true;
}

auto interop::compute_buffer_get_data(std::uint64_t id, void* data, std::int32_t byte_count) -> managed::bool32 {
  auto* state = find_buffer(id, "compute_buffer_get_data");

  if (!state) {
    return false;
  }

  if (!data || byte_count <= 0) {
    return true;
  }

  auto& buffer = core::engine::get_module<graphics::graphics_module>().resource_registry().get<graphics::buffer>(state->handle);

  if (static_cast<graphics::buffer::size_type>(byte_count) > buffer.size()) {
    utility::logger<"scripting">::error("compute_buffer_get_data: {} bytes exceed buffer {}'s size {}", byte_count, id, buffer.size());
    return false;
  }

  std::memcpy(data, buffer.mapped(), static_cast<std::size_t>(byte_count));

  return true;
}

auto interop::compute_buffer_release(std::uint64_t id) -> void {
  auto& registry = core::engine::get_module<scripting_module>().resources().compute_buffers;
  const auto entry = registry.find(id);

  if (entry == registry.end()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  // retire() requires a non-decreasing timeline across the pool, so pass the real frame index.
  graphics_module.resource_registry().retire(entry->second.handle, graphics_module.frame_context().frame_index());

  registry.erase(entry);
}

auto interop::compute_shader_load(managed::string path) -> std::uint64_t {
  const auto resolved = core::engine::project().assets_directory() / std::filesystem::path{std::string{path}};

  if (!std::filesystem::exists(resolved)) {
    utility::logger<"scripting">::error("compute_shader_load: '{}' does not exist", resolved.string());
    return 0u;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto state = compute_shader_state{.path = resolved};

  try {
    const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
      {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
    };

    const auto shader = graphics_module.shader_cache().get({resolved, entry_points});

    state.pipeline = graphics_module.compute_pipeline_cache().get(graphics::compute_pipeline::create_info{.shader = shader, .name = "Script Compute Shader"});
    state.layout = graphics_module.shader_compiler().reflect_push_constants(resolved, "compute_main");
  } catch (const std::exception& exception) {
    utility::logger<"scripting">::error("compute_shader_load: '{}': {}", resolved.string(), exception.what());
    return 0u;
  }

  if (state.layout.size > graphics::bindless_table::push_constant_size) {
    utility::logger<"scripting">::error("compute_shader_load: '{}' push_data is {} bytes, the push-constant range is {}", resolved.string(), state.layout.size, graphics::bindless_table::push_constant_size);
    return 0u;
  }

  state.params.resize(state.layout.size);
  state.is_set.resize(state.layout.fields.size(), false);

  const auto id = math::uuid::create().value();
  core::engine::get_module<scripting_module>().resources().compute_shaders.emplace(id, std::move(state));

  return id;
}

auto interop::compute_shader_set_value(std::uint64_t id, managed::string name, std::uint32_t kind, const void* data) -> managed::bool32 {
  auto* state = find_shader(id, "compute_shader_set_value");

  if (!state || !data) {
    return false;
  }

  const auto expected = static_cast<push_constant_field::kind>(kind);

  if (expected > push_constant_field::kind::f32x4) {
    utility::logger<"scripting">::error("compute_shader_set_value: kind {} is not a value kind", kind);
    return false;
  }

  const auto index = find_field(*state, std::string{name}, expected);

  if (!index) {
    return false;
  }

  write_field(*state, *index, data);

  return true;
}

auto interop::compute_shader_set_texture(std::uint64_t id, managed::string name, std::uint64_t texture_uuid, bool storage) -> managed::bool32 {
  auto* state = find_shader(id, "compute_shader_set_texture");

  if (!state) {
    return false;
  }

  const auto field_name = std::string{name};
  const auto index = find_field(*state, field_name, storage ? push_constant_field::kind::storage_texture : push_constant_field::kind::sampled_texture);

  if (!index) {
    return false;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto texture = assets_module.find_texture(math::uuid::from_value(texture_uuid));

  if (!texture.is_valid()) {
    utility::logger<"scripting">::error("ComputeShader '{}': texture {} for field '{}' does not resolve", state->path.filename().string(), texture_uuid, field_name);
    return false;
  }

  const auto texture_index = storage ? texture->storage_index() : texture->index();

  if (texture_index == assets::texture2d::invalid_index) {
    utility::logger<"scripting">::error("ComputeShader '{}': texture for storage field '{}' was not created via CreateStorageImage", state->path.filename().string(), field_name);
    return false;
  }

  if (!storage && !assets_module.is_resident(texture)) {
    utility::logger<"scripting">::warn("ComputeShader '{}': texture for field '{}' is not resident yet, the dispatch will sample placeholder data (gate on Texture2D.IsResident)", state->path.filename().string(), field_name);
  }

  write_field(*state, *index, &texture_index);

  return true;
}

auto interop::compute_shader_set_buffer(std::uint64_t id, managed::string name, std::uint64_t buffer_id) -> managed::bool32 {
  auto* state = find_shader(id, "compute_shader_set_buffer");
  auto* buffer_state = find_buffer(buffer_id, "compute_shader_set_buffer");

  if (!state || !buffer_state) {
    return false;
  }

  const auto field_name = std::string{name};
  const auto index = find_field(*state, field_name, push_constant_field::kind::buffer);

  if (!index) {
    return false;
  }

  const auto expected_stride = state->layout.fields[*index].element_stride;

  if (buffer_state->stride != expected_stride) {
    utility::logger<"scripting">::error("ComputeShader '{}': field '{}' points at {}-byte elements, but the buffer's element is {} bytes", state->path.filename().string(), field_name, expected_stride, buffer_state->stride);
    return false;
  }

  const auto& buffer = core::engine::get_module<graphics::graphics_module>().resource_registry().get<graphics::buffer>(buffer_state->handle);
  const auto address = buffer.address();

  write_field(*state, *index, &address);

  return true;
}

auto interop::compute_shader_release(std::uint64_t id) -> void {
  core::engine::get_module<scripting_module>().resources().compute_shaders.erase(id);
}

auto interop::compute_commands_begin() -> std::uint64_t {
  auto& bindless_table = core::engine::get_module<graphics::graphics_module>().bindless_table();

  auto state = compute_commands_state{graphics::command_buffer{graphics::queue::type::compute, true}};

  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(state.command_buffer.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  const auto id = math::uuid::create().value();
  core::engine::get_module<scripting_module>().resources().compute_commands.emplace(id, std::move(state));

  return id;
}

auto interop::compute_commands_dispatch(std::uint64_t id, std::uint64_t shader_id, std::uint32_t group_count_x, std::uint32_t group_count_y, std::uint32_t group_count_z) -> managed::bool32 {
  auto& registry = core::engine::get_module<scripting_module>().resources().compute_commands;
  const auto entry = registry.find(id);

  if (entry == registry.end()) {
    utility::logger<"scripting">::error("compute_commands_dispatch: unknown (or already submitted) command list {}", id);
    return false;
  }

  if (entry->second.fence) {
    utility::logger<"scripting">::error("compute_commands_dispatch: command list {} was already submitted", id);
    return false;
  }

  auto* shader = find_shader(shader_id, "compute_commands_dispatch");

  if (!shader) {
    return false;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  auto missing = std::string{};

  for (auto index = std::size_t{0u}; index < shader->layout.fields.size(); ++index) {
    const auto& field = shader->layout.fields[index];

    if (shader->is_set[index]) {
      continue;
    }

    if (field.type == push_constant_field::kind::sampler) {
      const auto sampler_index = bindless_table.sampler_index(graphics::sampler::create_info{
        .address_mode_u = graphics::address_mode::clamp_to_edge,
        .address_mode_v = graphics::address_mode::clamp_to_edge,
        .address_mode_w = graphics::address_mode::clamp_to_edge,
        .name = "Script Compute Default Sampler"
      });

      std::memcpy(shader->params.data() + field.offset, &sampler_index, sizeof(sampler_index));

      continue;
    }

    missing += missing.empty() ? field.name : ", " + field.name;
  }

  if (!missing.empty()) {
    utility::logger<"scripting">::error("ComputeShader '{}': dispatch with unset fields: {}", shader->path.filename().string(), missing);
    return false;
  }

  auto& state = entry->second;
  auto& command_buffer = state.command_buffer;

  // ponytail: full barrier between consecutive dispatches; track per-resource reads/writes like render_graph if independent dispatches ever need to overlap.
  if (state.dispatch_count > 0u) {
    auto barrier = VkMemoryBarrier2{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    command_buffer.memory_dependency(barrier);
  }

  command_buffer.bind_pipeline(*shader->pipeline);

  if (!shader->params.empty()) {
    command_buffer.push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, std::span<const std::byte>{shader->params});
  }

  command_buffer.dispatch(group_count_x, group_count_y, group_count_z);

  ++state.dispatch_count;

  return true;
}

auto interop::compute_commands_submit(std::uint64_t id, bool wait) -> managed::bool32 {
  auto& registry = core::engine::get_module<scripting_module>().resources().compute_commands;
  const auto entry = registry.find(id);

  if (entry == registry.end() || entry->second.fence) {
    utility::logger<"scripting">::error("compute_commands_submit: unknown (or already submitted) command list {}", id);
    return false;
  }

  auto& command_buffer = entry->second.command_buffer;

  // Makes the dispatches' writes visible to later compute, transfer (ReadPixels) and host (GetData) reads on this queue.
  // The graphics queue is covered by Submit() waiting on the host or by submit_async's timeline.
  auto barrier = VkMemoryBarrier2{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_HOST_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_HOST_READ_BIT;
  command_buffer.memory_dependency(barrier);

  if (wait) {
    command_buffer.submit_idle();
    registry.erase(entry);

    return true;
  }

  auto& fence = entry->second.fence.emplace();

  core::engine::get_module<graphics::graphics_module>().frame_context().submit_async(command_buffer, fence);

  return true;
}

auto interop::compute_commands_is_complete(std::uint64_t id) -> managed::bool32 {
  auto& registry = core::engine::get_module<scripting_module>().resources().compute_commands;
  const auto entry = registry.find(id);

  if (entry == registry.end()) {
    return true;
  }

  return entry->second.fence && entry->second.fence->is_signaled();
}

auto interop::compute_commands_release(std::uint64_t id) -> void {
  auto& registry = core::engine::get_module<scripting_module>().resources().compute_commands;
  const auto entry = registry.find(id);

  if (entry == registry.end()) {
    return;
  }

  // The command buffer can't be freed while the GPU may still execute it.
  if (entry->second.fence) {
    entry->second.fence->wait();
  }

  registry.erase(entry);
}

} // namespace sbx::scripting
