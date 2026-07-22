// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_PROFILER_HPP_
#define LIBSBX_GRAPHICS_PROFILER_HPP_

#if defined(SBX_ENABLE_PROFILING)

#include <vulkan/vulkan.h>

#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
#include <tracy/TracyVulkan.hpp>

#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>

namespace sbx::graphics::detail {

auto register_gpu_context(const queue::type type, std::string_view name, const instance& instance, const physical_device& physical_device, const logical_device& logical_device) -> void;

auto destroy_gpu_contexts() -> void;

auto gpu_context(const queue::type queue) noexcept -> TracyVkCtx;

} // namespace sbx::graphics::detail

#define SBX_PROFILE_GPU_CONTEXT_CREATE(type, name, instance, physical_device, logical_device) \
  sbx::graphics::detail::register_gpu_context(type, name, instance, physical_device, logical_device)

#define SBX_PROFILE_GPU_CONTEXT_DESTROY() \
  sbx::graphics::detail::destroy_gpu_contexts()

#define SBX_PROFILE_GPU_COLLECT(command_buffer) \
  TracyVkCollect(sbx::graphics::detail::gpu_context(command_buffer.type()), command_buffer)

#define SBX_PROFILE_GPU_SCOPE(command_buffer, name) \
  TracyVkZone(sbx::graphics::detail::gpu_context(command_buffer.type()), command_buffer, name)

#define SBX_PROFILE_GPU_SCOPE_COLORED(command_buffer, name, color) \
  TracyVkZoneC(sbx::graphics::detail::gpu_context(command_buffer.type()), command_buffer, name, color)

#else

#define SBX_PROFILE_GPU_CONTEXT_CREATE(type, name, instance, physical_device, logical_device) static_cast<void>(0)
#define SBX_PROFILE_GPU_CONTEXT_DESTROY() static_cast<void>(0)
#define SBX_PROFILE_GPU_COLLECT(command_buffer) static_cast<void>(0)
#define SBX_PROFILE_GPU_SCOPE(command_buffer, name) static_cast<void>(0)
#define SBX_PROFILE_GPU_SCOPE_COLORED(command_buffer, name, color) static_cast<void>(0)

#endif // SBX_ENABLE_PROFILING

#endif // LIBSBX_GRAPHICS_PROFILER_HPP_
