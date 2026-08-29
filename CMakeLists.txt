cmake_minimum_required(VERSION 3.25)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

project(libsbx VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_PROJECT_NAME_UPPER "SBX" CACHE STRING "CMAKE_PROJECT_NAME_UPPER" FORCE)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/dependencies" "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/cmake/modules")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/dependencies")

# Build type

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(SBX_BUILD_TYPE "DEBUG")
else()
  set(SBX_BUILD_TYPE "RELEASE")
endif()

# Operating system

if(WIN32)
  set(SBX_PLATFORM "WIN32")
elseif(APPLE)
  set(SBX_PLATFORM "APPLE")
elseif(UNIX)
  set(SBX_PLATFORM "LINUX")
else()
  message(FATAL_ERROR "Unsupported operating system")
endif()

message(STATUS "SBX_PLATFORM: ${SBX_PLATFORM}")

# Compiler

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(SBX_COMPILER "CLANG")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(SBX_COMPILER "GNU")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(SBX_COMPILER "MSC")
else()
  message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

message(STATUS "SBX_COMPILER: ${SBX_COMPILER}")

set(_SBX_ENABLE_PROFILING_DEFAULT OFF)

if(SBX_BUILD_TYPE STREQUAL "DEBUG")
  set(_SBX_ENABLE_PROFILING_DEFAULT ON)
endif()

# Options

option(SBX_BUILD_RUNTIME "Build runtime" ON)
option(SBX_BUILD_EDITOR "Build editor" ON)
option(SBX_BUILD_LAUNCHER "Build launcher" ON)
option(SBX_BUILD_TESTS "Build tests" ON)
option(SBX_BUILD_SHARED "Build shared library" OFF)
option(SBX_ENABLE_PROFILING "Enable tracy profiling" ${_SBX_ENABLE_PROFILING_DEFAULT})
option(SBX_PREFER_CPU_DEVICE "Prefer CPU device" OFF)

message(STATUS "SBX_BUILD_RUNTIME: ${SBX_BUILD_RUNTIME}")
message(STATUS "SBX_BUILD_EDITOR: ${SBX_BUILD_EDITOR}")
message(STATUS "SBX_BUILD_LAUNCHER: ${SBX_BUILD_LAUNCHER}")
message(STATUS "SBX_BUILD_TESTS: ${SBX_BUILD_TESTS}")
message(STATUS "SBX_BUILD_SHARED: ${SBX_BUILD_SHARED}")
message(STATUS "SBX_ENABLE_PROFILING: ${SBX_ENABLE_PROFILING}")
message(STATUS "SBX_PREFER_CPU_DEVICE: ${SBX_PREFER_CPU_DEVICE}")

# Build metadata

string(TIMESTAMP COMPILE_TIMESTAMP %Y%m%d-%H%M%S)
set(SBX_COMPILE_TIMESTAMP "${COMPILE_TIMESTAMP}" CACHE STRING "Compile timestamp" FORCE)

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE SBX_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE SBX_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
else()
  set(SBX_COMMIT_HASH "unknown")
  set(SBX_BRANCH "unknown")
endif()

add_compile_definitions(
  SBX_COMMIT_HASH="${SBX_COMMIT_HASH}"
  SBX_BRANCH="${SBX_BRANCH}"
)

message(STATUS "Git branch: ${SBX_BRANCH}")
message(STATUS "Git commit: ${SBX_COMMIT_HASH}")

set(SBX_SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SBX_SHADER_OUT_DIR ${CMAKE_BINARY_DIR}/shaders)

set(SBX_DOTNET_OUT_DIR ${CMAKE_BINARY_DIR}/dotnet)

# External dependencies

# System packages
find_package(Nethost REQUIRED MODULE)
find_package(slang REQUIRED MODULE)
find_package(Vulkan REQUIRED)

# Conan packages
find_package(fastgltf CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

# Configure version header

configure_file("${CMAKE_SOURCE_DIR}/libsbx/version.hpp.in" "${CMAKE_BINARY_DIR}/generated/libsbx/version.hpp" @ONLY)

# Library target

if(SBX_BUILD_SHARED)
  add_library(libsbx SHARED)
else()
  add_library(libsbx STATIC)
endif()

add_library(libsbx::libsbx ALIAS libsbx)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

target_sources(
  libsbx
  PRIVATE
    "${CMAKE_SOURCE_DIR}/libsbx/assets/asset_cooker.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/assets/asset_residency.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/assets/assets_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/assets/ibl_baker.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/core/core.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/core/engine.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/core/project.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/core/projects_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/core/user_data_directory.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/ecs/ecs.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/filesystem/filesystem_module.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/graphics/commands/command_buffer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/commands/command_pool.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/commands/mip_chain.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/allocator.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/debug_messenger.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/extensions.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/features.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/instance.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/layers.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/logical_device.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/physical_device.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/surface.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/swapchain.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/compute_pipeline.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/compute_pipeline_cache.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/graphics_pipeline.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/pipeline_binary_cache.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/pipeline_cache.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/shader_cache.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/shader_compiler.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/shader_disk_cache.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/shader.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/resources/buffer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/resources/image.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/resources/sampler.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/bindless_table.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/frame_context.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/graphics_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/profiler.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/upload_context.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/validate.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/math/color.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/math.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/matrix_cast.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/random.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/ray.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/uuid.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/memory/memory.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/platform/input.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/platform/platform_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/platform/process.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/platform/window.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/reflection/reflection.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/render/particles/particle_draw_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/particles/particle_pool.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/particles/particle_simulate_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/passes/depth_pre_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/passes/grid_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/passes/light_culling_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/passes/opaque_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/passes/skybox_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/passes/tonemap_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/passes/transparent_accumulate_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/passes/transparent_resolve_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/compositor.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/presentation_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/render_graph.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/render_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/render_thread.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/scene_blit_compositor.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/scene_renderer_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/shadow/cascade.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/shadow/shadow_pass.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/ui/ui_draw_data.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/ui/ui_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/ui/ui_system.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/ui/backends/v1.92.9-docking/imgui_impl_glfw.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/ui/backends/v1.92.9-docking/imgui_impl_vulkan.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/ui/fonts/embedded_fonts.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/render/ui/widgets/file_dialog.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene_serializer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/scenes_module.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/scripting/interop.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/scripting.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/scripting_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/assembly.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/attribute.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/detail/type_cache.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/field_info.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/garbage_collection.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/memory.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/method_info.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/object.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/property_info.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/runtime.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/string.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/type.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/signals/signals.cpp"

    "${CMAKE_SOURCE_DIR}/libsbx/utility/compression.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/utility/crc32.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/utility/timer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/utility/utility.cpp"

  PUBLIC
    FILE_SET headers TYPE HEADERS
    BASE_DIRS "${CMAKE_SOURCE_DIR}"
    FILES
      "${CMAKE_SOURCE_DIR}/libsbx/assets/asset_cooker.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/asset_handle.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/asset_residency.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/assets_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/environment_map.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/ibl_baker.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/material.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/mesh.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/particle_effect.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/texture.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/containers/compressed_pair.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/containers.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/dense_map.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/octree.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/ring_buffer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/stable_vector.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/static_vector.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/core/application.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/concepts.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/core.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/delegate.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/engine.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/engine.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/engine_config.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/exit.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/project.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/projects_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/threading_policy.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/user_data_directory.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/ecs/component.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/ecs.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/entity.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/entity.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/meta.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/range.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/registry.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/sparse_set.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/sparse_set.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/storage.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/storage.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/view.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/zip.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/detail/registry_storage_iterator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/detail/sparse_set_iterator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/detail/storage_iterator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/detail/view_iterator.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/alias.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/file_base.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/file_info.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/filesystem.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/filesystem_base.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/filesystem_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/memory_file.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/memory_filesystem.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/native_file.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/native_filesystem.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/virtual_filesystem.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/graphics/commands/command_buffer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/commands/command_pool.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/commands/mip_chain.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/allocator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/debug_messenger.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/extensions.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/features.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/instance.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/layers.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/logical_device.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/object_type.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/physical_device.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/surface.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/swapchain.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/graphics_pipeline.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/pipeline_binary_cache.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/pipeline_cache.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/shader_cache.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/shader_compiler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/shader_disk_cache.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/shader.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/resources/buffer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/resources/image.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/resources/resource_handle.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/resources/resource_pool.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/resources/resource_registry.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/resources/sampler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/bindless_table.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/frame_context.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/graphics_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/graphics.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/profiler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/types.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/upload_context.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/validate.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/math/algorithm.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/algorithm.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/angle.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/angle.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/box.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/box.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/color.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/concepts.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/constants.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/half.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/ieee_754_float.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/math.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix3x3.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix3x3.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix4x4.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix4x4.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix_cast.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix_cast.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/noise.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/plane.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/quaternion.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/quaternion.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/random.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/ray.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/smooth_value.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/sphere.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/traits.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/uuid.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector2.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector2.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector3.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector3.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector4.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector4.ipp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/volume.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/memory/aligned_storage.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/cache.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/concepts.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/counting_resource.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/iterable_adaptor.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/memory.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/observer_ptr.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/platform/events.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/platform/input.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/platform/input_action.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/platform/input_mod.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/platform/key.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/platform/mouse_button.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/platform/platform_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/platform/process.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/platform/window.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/reflection/enum.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/reflection/reflection.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/render/particles/particle_data.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/particles/particle_draw_pass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/particles/particle_pool.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/particles/particle_simulate_pass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/passes/depth_pre_pass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/passes/light_culling_pass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/passes/opaque_pass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/passes/skybox_pass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/passes/tonemap_pass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/passes/transparent_accumulate_pass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/passes/transparent_resolve_pass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/compositor.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/presentation_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/render_graph.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/render_packet.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/render_pass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/render_thread.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/scene_blit_compositor.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/scene_renderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/scene_renderer_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/ui_renderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/ui/ui_draw_data.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/ui/ui_layer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/ui/ui_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/ui/fonts/material_design_icons.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/ui/ui_system.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/render/ui/widgets/file_dialog.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene_serializer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/scenes_module.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/scripting/interop.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/scripting.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/scripting_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/assembly.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/attribute.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/core.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/field_info.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/fwd.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/garbage_collection.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/memory.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/message_type.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/method_info.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/object.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/platform.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/property_info.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/runtime.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/string.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/type.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/detail/backend.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/detail/type_cache.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/signals/cleanable.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/connection.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/copy_on_write.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/function_traits.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/object_ptr.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/observer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/signal.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/signals.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/slot.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/slot_state.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/to_weak.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/units/area.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/dimension.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/length.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/mass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/quantity.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/time.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/units.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/velocity.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/volume.hpp"

      "${CMAKE_SOURCE_DIR}/libsbx/utility/algorithm.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/assert.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/bit.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/cast.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/compression.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/concepts.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/crc32.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/enable_private_constructor.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/exception.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/fast_mod.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/fourcc.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/hashed_string.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/hash.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/iterator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/layout.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/lockable.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/logger.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/make_array.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/multimap_key_range.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/noncopyable.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/overload.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/primitive.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/profiler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/small_string.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/static_string.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/string_literal.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/target.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/timer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/type_id.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/type_list.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/type_name.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/utility.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/zip.hpp"

  FILE_SET generated TYPE HEADERS
    BASE_DIRS "${CMAKE_BINARY_DIR}/generated"
    FILES
      "${CMAKE_BINARY_DIR}/generated/libsbx/version.hpp"
)

target_include_directories(
  libsbx
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>"
    "$<INSTALL_INTERFACE:include>"
)

target_link_libraries(
  libsbx
  PUBLIC
    fastgltf::fastgltf
    fmt::fmt
    glfw
    GPUOpen::VulkanMemoryAllocator
    imgui::imgui
    imguizmo::imguizmo
    lz4::lz4
    meshoptimizer::meshoptimizer
    Nethost::nethost
    slang::slang
    spdlog::spdlog
    stb::stb
    Tracy::TracyClient
    Vulkan::Headers
    Vulkan::Vulkan
    yaml-cpp::yaml-cpp
    zstd::libzstd
)

if(SBX_ENABLE_PROFILING)
  target_link_libraries(libsbx PUBLIC TracyClient)
endif()

set_target_properties(
  libsbx
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME sbx
)

if(MSVC)
  set_target_properties(libsbx PROPERTIES PREFIX "lib")
endif()

target_compile_definitions(
  libsbx
  PUBLIC
    SBX_COMPILE_TIMESTAMP="${SBX_COMPILE_TIMESTAMP}"
    SBX_COMMIT_HASH="${SBX_COMMIT_HASH}"
    SBX_BRANCH="${SBX_BRANCH}"
    SBX_PLATFORM_${SBX_PLATFORM}
    SBX_COMPILER_${SBX_COMPILER}
    SBX_BUILD_TYPE_${SBX_BUILD_TYPE}
    $<$<BOOL:${SBX_ENABLE_PROFILING}>:SBX_ENABLE_PROFILING>
    $<$<BOOL:${SBX_PREFER_CPU_DEVICE}>:SBX_PREFER_CPU_DEVICE>
  PRIVATE
    $<$<BOOL:${SBX_ENABLE_PROFILING}>:TRACY_ENABLE>
)

set(_sbx_warning_options_gnu_clang
  -Wall
  -Wextra
  -Wpedantic
  -Wcast-align
  -Wconversion
  -Wdouble-promotion
  -Wformat=2
  -Wnon-virtual-dtor
  -Wnull-dereference
  -Wold-style-cast
  -Woverloaded-virtual
  -Wsign-conversion
  -Wunused
)

set(_sbx_warning_options_gnu
  -Wduplicated-branches
  -Wduplicated-cond
  -Wlogical-op
  -Wuseless-cast
)

set(_sbx_warning_options_msvc
  /W4
  /permissive-
)

target_compile_options(
  libsbx
  PUBLIC
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-freflection>
  PRIVATE
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:${_sbx_warning_options_gnu_clang}>"
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU>:${_sbx_warning_options_gnu}>"
    "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:${_sbx_warning_options_msvc}>"
)

set(_sbx_link_options)

if(MINGW)
  list(APPEND _sbx_link_options -Wl,--disable-dynamicbase,--default-image-base-low)
endif()

target_link_options(libsbx PUBLIC ${_sbx_link_options})

# Scripting: dotnet build targets

set(SBX_MANAGED_DIR ${CMAKE_SOURCE_DIR}/dotnet/Sbx.Managed)
set(SBX_MANAGED_PROJECT ${SBX_MANAGED_DIR}/Sbx.Managed.csproj)

add_custom_target(
  sbx_scripting_managed ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SBX_DOTNET_OUT_DIR}
  COMMAND dotnet publish ${SBX_MANAGED_PROJECT}
    -c $<CONFIG>
    --self-contained false
    -p:UseAppHost=false
    -o ${SBX_DOTNET_OUT_DIR}
  WORKING_DIRECTORY ${SBX_MANAGED_DIR}
  COMMENT "Publishing Sbx.Managed assembly (framework-dependent)"
)

add_dependencies(libsbx sbx_scripting_managed)

set(SBX_CORE_DIR ${CMAKE_SOURCE_DIR}/dotnet/Sbx.Core)
set(SBX_CORE_PROJECT ${SBX_CORE_DIR}/Sbx.Core.csproj)

add_custom_target(
  sbx_scripting_core ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SBX_DOTNET_OUT_DIR}
  COMMAND dotnet publish ${SBX_CORE_PROJECT}
    -c $<CONFIG>
    --self-contained false
    -p:UseAppHost=false
    -o ${SBX_DOTNET_OUT_DIR}
  WORKING_DIRECTORY ${SBX_CORE_DIR}
  COMMENT "Publishing Sbx.Core assembly (self-contained ${SBX_DOTNET_RID})"
)

add_dependencies(sbx_scripting_core sbx_scripting_managed)
add_dependencies(libsbx sbx_scripting_core)

# Install

install(
  TARGETS libsbx
  EXPORT libsbxTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  FILE_SET headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILE_SET generated DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  EXPORT libsbxTargets
  NAMESPACE libsbx::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libsbx
)

write_basic_package_version_file(
  libsbxConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  ${CMAKE_SOURCE_DIR}/cmake/config/libsbxConfig.cmake.in
  ${CMAKE_BINARY_DIR}/libsbxConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libsbx
)

install(
  FILES
    ${CMAKE_BINARY_DIR}/libsbxConfig.cmake
    ${CMAKE_BINARY_DIR}/libsbxConfigVersion.cmake
  DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/libsbx
)

install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/cmake/modules/
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libsbx/modules
  FILES_MATCHING PATTERN "Find*.cmake"
)

# install(CODE "
#   if(NOT EXISTS \"${SBX_SHADER_OUT_DIR}/manifest.txt\")
#     message(FATAL_ERROR \"Engine data not staged. Run 'cmake --build' before 'cmake --install'.\")
#   endif()
# ")

install(
  DIRECTORY ${SBX_DOTNET_OUT_DIR}
  DESTINATION ${CMAKE_INSTALL_DATADIR}/libsbx
  USE_SOURCE_PERMISSIONS
  PATTERN "*.pdb" EXCLUDE
)


# install(
#   FILES $<TARGET_FILE:slang::slang>
#   DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )

# Demo

if(SBX_BUILD_RUNTIME AND EXISTS "${CMAKE_SOURCE_DIR}/runtime")
  add_subdirectory(runtime)
endif()

if (SBX_BUILD_EDITOR AND EXISTS "${CMAKE_SOURCE_DIR}/editor")
  add_subdirectory(editor)
endif()

if (SBX_BUILD_LAUNCHER AND EXISTS "${CMAKE_SOURCE_DIR}/launcher")
  add_subdirectory(launcher)
endif()
