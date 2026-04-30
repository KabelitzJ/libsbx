cmake_minimum_required(VERSION 3.20)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

project(libsbx VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_PROJECT_NAME_UPPER "SBX" CACHE STRING "CMAKE_PROJECT_NAME_UPPER" FORCE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/dependencies" "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/cmake/modules")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/dependencies")

# Options

option(SBX_BUILD_DEMO "Build demo" ON)
option(SBX_BUILD_EDITOR "Build editor" ON)
option(SBX_BUILD_SHARED "Build shared library" OFF)
option(SBX_BUILD_TESTS "Build tests" ON)
option(SBX_CONSTEXPR_ENABLED "Enable constexpr" ON)

set(_PROFILING_ENABLED_DEFAULT OFF)
set(_ASAN_ENABLED_DEFAULT OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_PROFILING_ENABLED_DEFAULT ON)
  set(_ASAN_ENABLED_DEFAULT ON)
endif()

option(SBX_PROFILING_ENABLED "Enable profiling" ${_PROFILING_ENABLED_DEFAULT})
option(SBX_ASAN_ENABLED "Enable AddressSanitizer" ${_ASAN_ENABLED_DEFAULT})

message(STATUS "SBX_BUILD_DEMO: ${SBX_BUILD_DEMO}")
message(STATUS "SBX_BUILD_EDITOR: ${SBX_BUILD_EDITOR}")
message(STATUS "SBX_BUILD_SHARED: ${SBX_BUILD_SHARED}")
message(STATUS "SBX_BUILD_TESTS: ${SBX_BUILD_TESTS}")
message(STATUS "SBX_CONSTEXPR_ENABLED: ${SBX_CONSTEXPR_ENABLED}")
message(STATUS "SBX_PROFILING_ENABLED: ${SBX_PROFILING_ENABLED}")
message(STATUS "SBX_ASAN_ENABLED: ${SBX_ASAN_ENABLED}")

# Build metadata

string(TIMESTAMP COMPILE_TIMESTAMP %Y%m%d-%H%M%S)
set(SBX_COMPILE_TIMESTAMP "\"${COMPILE_TIMESTAMP}\"" CACHE STRING "Compile timestamp" FORCE)
add_compile_definitions(SBX_COMPILE_TIMESTAMP=${SBX_COMPILE_TIMESTAMP})

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

set(SBX_DATA_DIR ${CMAKE_BINARY_DIR}/data)

set(SBX_SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SBX_SHADER_OUT_DIR ${SBX_DATA_DIR}/shaders)

set(DOTNET_OUT_DIR ${SBX_DATA_DIR}/dotnet)

# External dependencies

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(range-v3 CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(spirv-cross CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(slang REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(base64 CONFIG REQUIRED)
find_package(tsl-robin-map CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(imnodes CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(drwav CONFIG REQUIRED)
find_package(OpenAL CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(Nethost REQUIRED)
find_package(Tracy CONFIG REQUIRED)

option(GLFW_BUILD_EXAMPLES "" OFF)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
option(GLFW_DOCUMENT_INTERNALS "" OFF)
option(SPIRV_CROSS_CLI "" OFF)
option(SPIRV_CROSS_ENABLE_TESTS "" OFF)

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

add_custom_target(
  compiled_shaders_libsbx ALL
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.py
    ${SBX_SHADER_SRC_DIR}
    --output ${SBX_SHADER_OUT_DIR}
    --manifest ${SBX_SHADER_SRC_DIR}/manifest.txt
  COMMENT "Compiling shaders for ${PROJECT_NAME}..."
)

add_custom_target(
  staged_shaders_libsbx ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${SBX_SHADER_SRC_DIR} ${SBX_SHADER_OUT_DIR}
  DEPENDS compiled_shaders_libsbx
  COMMENT "Staging shader sources for ${PROJECT_NAME}..."
)

add_dependencies(${PROJECT_NAME} staged_shaders_libsbx)

target_sources(
  libsbx
  PRIVATE
    "${CMAKE_SOURCE_DIR}/libsbx/animations/animation.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/animations/animations_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/animations/animations.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/animations/animator.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/animations/mesh.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/animations/skeleton.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/animations/skinned_mesh_material_subrenderer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/animations/skinned_mesh_shadow_subrenderer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/animations/skinning_task.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/animations/spline.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/assets/assets.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/audio/audio_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/audio/audio.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/bitmaps/bitmap.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/bitmaps/bitmaps.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/bitmaps/loaders/jpg_loader.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/bitmaps/loaders/png_loader.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/containers/containers.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/core/core.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/core/engine.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/devices/devices.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/devices/input.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/devices/window.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ecs/ecs.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ecs/entity.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/ecs/sparse_set.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/ecs/storage.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/filesystem/filesystem_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/gizmos/gizmos.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/buffer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/push_handler.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/push_handler.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/storage_buffer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/storage_handler.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/storage_handler.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/uniform_buffer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/uniform_handler.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/uniform_handler.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/commands/command_buffer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/commands/command_pool.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/descriptor/descriptor_handler.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/descriptor/descriptor_set.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/descriptor/descriptor.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/allocator.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/debug_messenger.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/extensions.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/instance.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/logical_device.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/physical_device.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/query_pool.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/surface.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/validation_layers.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/draw_list.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/draw_list.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/graphics_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/cube_image.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/depth_image.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/image.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/image2d.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/sampler_state.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/separate_image2d_array.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/separate_sampler_array.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/compiler.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/compute_pipeline.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/graphics_pipeline.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/mesh.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/shader.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/profiler.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/render_graph.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/render_graph.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/graphics/render_pass/swapchain.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/io/io.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/io/read_file.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/algorithm.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/angle.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/box.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/color.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/math.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/matrix_cast.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/matrix_cast.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/matrix.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/matrix3x3.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/matrix4x4.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/quaternion.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/random.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/ray.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/uuid.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/vector.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/vector2.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/vector3.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/math/vector4.ipp"
    "${CMAKE_SOURCE_DIR}/libsbx/memory/memory.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/memory/tracking_allocator.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/models/frustum_culling_task.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/models/material.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/models/mesh.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/models/models_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/models/models.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/models/static_mesh_material_subrenderer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/models/static_mesh_shadow_subrenderer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/particles/particle_subrenderer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/particles/particle_task.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/particles/particles_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/particles/particles.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/physics/character_controller.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/physics/collision_detection.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/physics/mesh_collider.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/physics/physics_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/physics/physics.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/physics/rigidbody.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/physics/shape_collider.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/post/post.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/reflection/reflection.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/camera.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/script.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/transform.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/node.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene_environment.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene_serializer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/scenes_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scenes/scenes.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/interop.cpp"
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
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/scripting_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/scripting/scripting.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/signals/signals.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/sprites/sprite_subrenderer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/sprites/sprites_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/sprites/sprites.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/button.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/canvas.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/element.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/font.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/grid_layout.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/horizontal_layout.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/image.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/label.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/layout.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/panel.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/ui_module.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/ui/ui.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/units/units.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/utility/compression.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/utility/crc32.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/utility/timer.cpp"
    "${CMAKE_SOURCE_DIR}/libsbx/utility/utility.cpp"
  PUBLIC
    FILE_SET headers TYPE HEADERS
    BASE_DIRS "${CMAKE_SOURCE_DIR}"
    FILES
      "${CMAKE_SOURCE_DIR}/libsbx/animations/animation.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/animations_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/animations.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/animator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/mesh.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/skeleton.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/skinned_mesh_material_draw_list.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/skinned_mesh_material_subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/skinned_mesh_shadow_subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/skinning_task.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/spline.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/animations/vertex3d.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/assets_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/assets.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/metadata.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/assets/thread_pool.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/audio/audio_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/audio/audio.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/bitmaps/bitmap.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/bitmaps/bitmaps.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/bitmaps/loaders/jpg_loader.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/bitmaps/loaders/png_loader.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/bimap.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/compressed_pair.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/containers.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/dense_map.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/octree.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/ring_buffer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/spsc_queue.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/stable_vector.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/static_vector.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/containers/task_graph.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/application.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/cli.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/concepts.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/core.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/delegate.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/engine.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/exit.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/profiler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/core/settings.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/devices/devices_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/devices/devices.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/devices/events.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/devices/input_action.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/devices/input_mod.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/devices/input.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/devices/key.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/devices/mouse_button.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/devices/window.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/component.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/detail/registry_storage_iterator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/detail/sparse_set_iterator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/detail/storage_iterator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/detail/view_iterator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/ecs.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/entity.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/meta.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/range.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/registry.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/sparse_set.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/storage.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/view.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ecs/zip.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/alias.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/file_base.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/file_info.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/filesystem_base.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/filesystem_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/memory_file.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/memory_filesystem.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/native_file.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/native_filesystem.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/filesystem/virtual_filesystem.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/gizmos/gizmos_subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/gizmos/gizmos.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/gizmos/pipeline.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/buffer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/push_handler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/storage_buffer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/storage_handler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/uniform_buffer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/buffers/uniform_handler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/commands/command_buffer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/commands/command_pool.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/descriptor/descriptor_handler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/descriptor/descriptor_set.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/descriptor/descriptor.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/allocator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/debug_messenger.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/extensions.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/instance.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/logical_device.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/physical_device.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/query_pool.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/surface.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/devices/validation_layers.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/draw_list.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/graphics_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/graphics.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/cube_image.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/depth_image.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/image.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/image2d.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/sampler_state.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/separate_image2d_array.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/images/separate_sampler_array.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/compiler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/compute_pipeline.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/graphics_pipeline.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/mesh.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/pipeline.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/shader.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/pipeline/vertex_input_description.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/profiler.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/render_graph.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/render_pass/swapchain.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/renderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/resource_storage.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/task.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/viewport_registry.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/graphics/viewport.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/io/concepts.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/io/io.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/io/loader_factory.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/io/read_file.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/algorithm.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/angle.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/box.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/color.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/concepts.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/constants.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/half.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/ieee_754_float.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/math.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix_cast.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix3x3.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/matrix4x4.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/noise.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/plane.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/quaternion.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/random.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/ray.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/smooth_value.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/sphere.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/traits.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/uuid.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector2.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector3.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/vector4.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/math/volume.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/aligned_storage.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/blob.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/cache.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/concepts.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/counting_resource.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/iterable_adaptor.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/memory.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/monotonic_arena.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/observer_ptr.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/memory/tracking_allocator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/frustum_culling_task.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/material_draw_list.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/material.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/mesh.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/models_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/models.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/static_mesh_material_draw_list.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/static_mesh_material_subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/static_mesh_shadow_subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/vertex_stream.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/models/vertex3d.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/particles/particle_emitter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/particles/particle_subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/particles/particle_task.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/particles/particles_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/particles/particles.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/physics/character_controller.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/physics/collision_detection.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/physics/mesh_collider.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/physics/physics_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/physics/physics.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/physics/rigidbody.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/physics/shape_collider.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filters/bloom_filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filters/blur_filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filters/default_filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filters/downsample_filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filters/fxaa_filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filters/resolve_filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filters/selection_filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filters/ssao_filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filters/tonemap_filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/filters/upsample_filter.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/post/post.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/reflection/description.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/reflection/enumerator.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/reflection/member.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/reflection/reflection.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/asset_io.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/asset_registry.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/component_io.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/camera.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/directional_light.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/gizmo.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/global_transform.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/id.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/point_light.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/relationship.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/script.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/skinned_mesh.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/skybox.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/static_mesh.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/tag.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/components/transform.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/debug_subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/grid_subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/node.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene_environment.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene_graph.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene_serializer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/scene.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/scenes_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/scenes.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scenes/skybox_subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/interop.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/assembly.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/attribute.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/core.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/detail/backend.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/managed/detail/type_cache.hpp"
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
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/scripting_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/scripting/scripting.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/cleanable.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/connection.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/copy_on_write.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/function_traits.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/object_ptr.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/observer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/signal.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/signals.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/slot_state.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/slot.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/signals/to_weak.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/sprites/sprite_subrenderer.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/sprites/sprites_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/sprites/sprites.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/button.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/canvas.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/element.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/font.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/grid_layout.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/horizontal_layout.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/image.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/label.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/layout.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/panel.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/ui_module.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/ui.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/ui/vertical_layout.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/bytes.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/distance.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/mass.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/quantity.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/time.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/units/units.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/algorithm.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/assert.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/bitmask.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/cast.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/compression.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/concepts.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/crc32.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/enable_private_constructor.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/enum.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/exception.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/expected.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/fast_mod.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/hash.hpp"
      "${CMAKE_SOURCE_DIR}/libsbx/utility/hashed_string.hpp"
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
    fmt::fmt
    spdlog::spdlog
    range-v3::range-v3
    lz4::lz4
    zstd::libzstd
    magic_enum::magic_enum
    yaml-cpp::yaml-cpp
    stb::stb
    Vulkan::Vulkan
    spirv-cross::spirv-cross
    GPUOpen::VulkanMemoryAllocator
    nlohmann_json::nlohmann_json
    slang::slang
    glfw
    tinyobjloader::tinyobjloader
    aklomp::base64
    tsl::robin_map
    assimp::assimp
    meshoptimizer::meshoptimizer
    imgui::imgui
    implot::implot
    imnodes::imnodes
    imguizmo::imguizmo
    Freetype::Freetype
    drwav::drwav
    OpenAL::OpenAL
    sol2::sol2
    Nethost::nethost
    Tracy::TracyClient
)

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

target_compile_features(libsbx PUBLIC cxx_std_23)

target_compile_definitions(
  libsbx
  PUBLIC
    SBX_CONSTEXPR_ENABLED=$<IF:$<BOOL:${SBX_CONSTEXPR_ENABLED}>,1,0>
    SBX_PROFILING_ENABLED=$<IF:$<BOOL:${SBX_PROFILING_ENABLED}>,1,0>
    $<$<BOOL:${SBX_PROFILING_ENABLED}>:TRACY_ENABLE>
  PRIVATE
    GLFW_INCLUDE_VULKAN
)

target_compile_options(
  libsbx
  PRIVATE
    -Wall
    -Wextra
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wuseless-cast
)

set(_sbx_link_options)
set(_sbx_compile_options)

if(SBX_USE_ASAN AND NOT MINGW)
  list(APPEND _sbx_compile_options -fsanitize=address,undefined -fno-omit-frame-pointer)
  list(APPEND _sbx_link_options -fsanitize=address,undefined)
endif()

if(MINGW)
  list(APPEND _sbx_link_options -Wl,--disable-dynamicbase,--default-image-base-low)
endif()

target_compile_options(libsbx PRIVATE ${_sbx_compile_options})
target_link_options(libsbx PUBLIC ${_sbx_link_options})

# Scripting: dotnet build targets

set(SBX_MANAGED_DIR ${CMAKE_SOURCE_DIR}/dotnet/Sbx.Managed)
set(SBX_MANAGED_PROJECT ${SBX_MANAGED_DIR}/Sbx.Managed.csproj)

add_custom_target(
  sbx_scripting_managed ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_OUT_DIR}
  COMMAND dotnet publish ${SBX_MANAGED_PROJECT}
    -c $<CONFIG>
    --self-contained false
    -p:UseAppHost=false
    -o ${DOTNET_OUT_DIR}
  WORKING_DIRECTORY ${SBX_MANAGED_DIR}
  COMMENT "Publishing Sbx.Managed assembly (framework-dependent)"
)

add_dependencies(libsbx sbx_scripting_managed)

set(SBX_CORE_DIR ${CMAKE_SOURCE_DIR}/dotnet/Sbx.Core)
set(SBX_CORE_PROJECT ${SBX_CORE_DIR}/Sbx.Core.csproj)

add_custom_target(
  sbx_scripting_core ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_OUT_DIR}
  COMMAND dotnet publish ${SBX_CORE_PROJECT}
    -c $<CONFIG>
    --self-contained false
    -p:UseAppHost=false
    -o ${DOTNET_OUT_DIR}
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

install(CODE "
  if(NOT EXISTS \"${SBX_SHADER_OUT_DIR}/manifest.txt\")
    message(FATAL_ERROR \"Engine data not staged. Run 'cmake --build' before 'cmake --install'.\")
  endif()
")

install(
  DIRECTORY ${SBX_DATA_DIR}/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/libsbx
  USE_SOURCE_PERMISSIONS
  PATTERN "*.pdb" EXCLUDE
)


install(
  FILES $<TARGET_FILE:slang::slang>
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Tests

if(SBX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# Demo

if(SBX_BUILD_DEMO)
  add_subdirectory(demo)
endif()

# Editor

if(SBX_BUILD_EDITOR)
  add_subdirectory(editor)
endif()
